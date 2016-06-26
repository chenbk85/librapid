//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <openssl/sha.h>

#include "openssl/sslmanager.h"

#include <rapid/utils/singleton.h>
#include <rapid/logging/logging.h>

#include "http1xcodec.h"
#include "http2/http2codec.h"
#include "websocket/websocketcodec.h"

#include "websocket/websocketservice.h"
#include "httpserverconfigfacade.h"
#include "httpconstants.h"
#include "httpcontext.h"

HttpContext::HttpContext()
	: hasUpgraded_(false)
	, id_(0) {
}

HttpContext::~HttpContext() {

}

void HttpContext::setEventHandler() {
	auto pHttpMethodDispatcher = std::make_shared<MessageDispatcher<HttpRequest>>();

	// HTTP method event handler
	pHttpMethodDispatcher->addMessageEventHandler(HTTP_METHOD_GET,
		[this](rapid::ConnectionPtr &conn, HttpRequestPtr httpRequest) {
		pHttpRequest_ = httpRequest;
		onHttpGetMessage(conn);
	});

	pHttpMethodDispatcher->addMessageEventHandler(HTTP_METHOD_POST,
		[this](rapid::ConnectionPtr &conn, HttpRequestPtr httpRequest) {
		pHttpRequest_ = httpRequest;
		onHttpPostMessage(conn);
	});

	// Load codec object
	if (!hasUpgraded_) {
		pHttpCodec_ = std::make_unique<Http1xCodec>(pHttpMethodDispatcher);
		pHttpResponse_ = HttpResponsePtr(HttpServerConfigFacade::getInstance().getHttpResponsePool().borrowObject());
	} else {
		pHttpCodec_ = std::make_unique<Http2Codec>(pHttpMethodDispatcher);
		pHttpResponse_ = HttpResponsePtr(HttpServerConfigFacade::getInstance().getHttp2ResponsePool().borrowObject());
	}
}

void HttpContext::onAcceptConnection(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();
	readLoop(pConn);
}

void HttpContext::handshake(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	setEventHandler();
	
	pConn->setReceiveEventHandler([this](rapid::ConnectionPtr conn) {
		readLoop(conn);
	});

	pConn->setSendEventHandler([this](rapid::ConnectionPtr conn) {
		sendMessage(conn);
	});

	hasUpgraded_ = false;

	onAcceptConnection(pConn);
}

void HttpContext::onHttpGetMessage(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

    // HTTP1.1 有些browser需要'Connection'來表示keep-alive!
    // HTTP2 中取消了'Connection' 
    if (!HttpServerConfigFacade::getInstance().isUseHttp2()) {
        pHttpResponse_->setKeepAlive(true);
    }

	if (!pHttpRequest_->isUpgradeRequest()) {
		sendMessage(pConn);
	} else {
		if (pHttpRequest_->isHttp2UpgradeRequest()) {
			sendSwitchToHttp2cMessage(pConn);
		}
		else if (pHttpRequest_->isWebSocketUpgradeRequest()) {
			sendSwitchToWebSocketMessage(pConn);
		}
		else {
			sendMessage(pConn);
		}
	}
}

void HttpContext::onHttpPostMessage(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

    // HTTP1.1 有些browser需要'Connection'來表示keep-alive!
    // HTTP2 中取消了'Connection' 
    if (!HttpServerConfigFacade::getInstance().isUseHttp2()) {
        pHttpResponse_->setKeepAlive(true);
    }

	pHttpRequest_->createTempFile();

	try {
		readPostData(pConn);
	} catch (HttpRequestErrorException const &e) {
		RAPID_LOG_ERROR() << e.what() << " error code: " << e.errorCode();
		pHttpResponse_->writeErrorResponseHeader(pConn->getSendBuffer(), e.errorCode());
		pConn->sendAndDisconnec();
	}	
}

void HttpContext::setId(size_t id) noexcept {
	id_ = id;
}

size_t HttpContext::id() const noexcept {
	return id_;
}

void HttpContext::readPostData(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	if (pHttpRequest_->writeTempFile(pConn)) {
		sendMessage(pConn);
	}
}

void HttpContext::readLoop(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	uint32_t bytesToRead = 0;
	auto pBuffer = pConn->getReceiveBuffer();

	do {
		if (!pBuffer->isEmpty()) {
			pHttpCodec_->readLoop(pConn, bytesToRead);
			if (!bytesToRead) break; // Decode done!
		}
	} while (pBuffer->readSome(pConn));
}

void HttpContext::sendMessage(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	while (!pHttpResponse_->send(pConn, pHttpRequest_)) {
		if (!pConn->sendAsync()) {
			return;
		}
	}

	pHttpResponse_->reset();
	pHttpRequest_->removeAll();
	readLoop(pConn);
}

void HttpContext::sendSwitchToHttp2cMessage(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	if (pHttpRequest_->has(HTTP_CONNECTION)) {
		auto value = pHttpRequest_->get(HTTP_CONNECTION);
		auto hasHttp2Settings = value.find(HTTP2_SETTINGS.str());
		if (hasHttp2Settings) {
			decodeBase64(pHttpRequest_->get(HTTP2_SETTINGS));
		}
	}

	pHttpResponse_->setStatusCode(HTTP_SWITCHING_PROTOCOLS);
	pHttpResponse_->add(HTTP_CONNECTION, HTTP_UPGRADE.str());
	pHttpResponse_->add(HTTP_UPGRADE, HTTP2_UPGRADE_PROTOCOL);
	pHttpResponse_->serialize(pConn->getSendBuffer());

	hasUpgraded_ = true;

	pConn->setSendEventHandler([this](rapid::ConnectionPtr conn) {
		handshake(conn);
	});

	if (pConn->sendAsync()) {
		handshake(pConn);
	}
}

void HttpContext::sendSwitchToWebSocketMessage(rapid::ConnectionPtr &pConn) {
	/*
	HTTP/1.1 101 Switching Protocols
	Upgrade: websocket
	Connection: Upgrade
	Sec-WebSocket-Accept: fFBooB7FAkLlXgRSz0BT3v4hq5s=
	Sec-WebSocket-Origin: null
	Sec-WebSocket-Location: ws://example.com/
	*/
	RAPID_TRACE_CALL();

	pHttpResponse_->setStatusCode(HTTP_SWITCHING_PROTOCOLS);
	pHttpResponse_->add(HTTP_UPGRADE, HTTP_WEBSOCKET.str());
	pHttpResponse_->add(HTTP_CONNECTION, HTTP_UPGRADE.str());
	pHttpResponse_->add(HTTP_SEC_WEBSOCKET_ACCEPT, getWebsocketAcceptKey(pHttpRequest_->get(HTTP_SEC_WEBSOCKET_KEY)));
	pHttpResponse_->add(HTTP_CONETNT_LENGTH, 0);

	std::string protocol;
	if (pHttpRequest_->has(HTTP_SEC_WEBSOCKET_PROTOCOL)) {
		protocol = pHttpRequest_->get(HTTP_SEC_WEBSOCKET_PROTOCOL);
		pHttpResponse_->add(HTTP_SEC_WEBSOCKET_PROTOCOL, static_cast<std::string const &>(protocol));
	}

	RAPID_LOG_TRACE() << "WebSocket protocol: " << protocol;

	pHttpResponse_->serialize(pConn->getSendBuffer());

	// Setup WebSocket service

	hasUpgraded_ = true;
	
	pWebSocketService_ = WebSocketService::createService(protocol);

	auto pWebSocketDispatcher = std::make_shared<MessageDispatcher<WebSocketRequest>>();
	
	pWebSocketDispatcher->addMessageEventHandler(WS_MESSAGE,
		[this](rapid::ConnectionPtr &conn, WebSocketRequestPtr pWebSocketRequest) {
		pWebSocketRequest_ = pWebSocketRequest;
		onWebSocketMessage(conn);
	});

	pHttpCodec_ = std::make_unique<WebSocketCodec>(pWebSocketDispatcher);
	auto pContext = shared_from_this();

	// Set handler to read WebSocket data
	pConn->setSendEventHandler([this, pContext](rapid::ConnectionPtr conn) {
		pWebSocketService_->onOpen(conn, pContext);
		readLoop(conn);
	});

	if (pConn->sendAsync()) {
		pWebSocketService_->onOpen(pConn, pContext);
		readLoop(pConn);
	}
}

void HttpContext::onWebSocketMessage(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	if (pWebSocketService_->onMessage(pConn, pWebSocketRequest_)) {
		readLoop(pConn);
	}
}

void HttpContext::onDisconnect(rapid::ConnectionPtr &pConn) {
	RAPID_TRACE_CALL();

	if (pWebSocketService_ != nullptr) {
		pWebSocketService_->onClose(pConn);
		pWebSocketService_.reset();
	}
}
