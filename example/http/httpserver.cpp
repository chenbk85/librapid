//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <iostream>

#include <rapid/utils/singleton.h>
#include "openssl/sslmanager.h"
#include "openssl/ssl.h"

#include <rapid/logging/logging.h>

#include "httpserverconfigfacade.h"
#include "httpserver.h"

std::unique_ptr<HttpServer> HttpServer::createHttpServer(std::string const &ipAddress, uint16_t port) {
	return std::make_unique<HttpServer>(ipAddress, port);
}

HttpServer::HttpServer(std::string const &ipAddress, uint16_t port)
	: server_(ipAddress, port) {
}

HttpServer::~HttpServer() {

}

void HttpServer::stop() {
	stopFlag_.notify_one();
}

void HttpServer::start() {
	RAPID_TRACE_CALL();

	server_.setSocketPool(
		HttpServerConfigFacade::getInstance().getInitalUserConnection(),
		HttpServerConfigFacade::getInstance().getMaxUserConnection(), 
		HttpServerConfigFacade::getInstance().getBufferSize());

	if (HttpServerConfigFacade::getInstance().isUseSSL()) {
		// Lazy initial OpenSSL library
		SSLInitializer::getInstance();

		RAPID_LOG_INFO() << "Starting init Open-SSL library...";

		SSLManager::getInstance().initializeServer(std::make_shared<SSLContext>(
			HttpServerConfigFacade::getInstance().getPrivateKeyFilePath(),
			HttpServerConfigFacade::getInstance().getCertificateFilePath()));

		SSLManager::getInstance().setALPNProtocol(
			HttpServerConfigFacade::getInstance().isUseHttp2()
			? ALPN_HTTP_V2 : ALPN_HTTP_V1_1);

		RAPID_LOG_INFO() << "Enable HTTPS";
	}

#ifdef _DEBUG
	static uint32_t constexpr DEBUG_POOL_SIZE = 200;

	HttpServerConfigFacade::getInstance().getHttpRequestPool().expand(DEBUG_POOL_SIZE);
	HttpServerConfigFacade::getInstance().getHttpResponsePool().expand(DEBUG_POOL_SIZE);

	if (HttpServerConfigFacade::getInstance().isUseSSL()) {
		HttpServerConfigFacade::getInstance().getHttpsContextPool().expand(DEBUG_POOL_SIZE);
	} else {
		HttpServerConfigFacade::getInstance().getHttpContextPool().expand(DEBUG_POOL_SIZE);
	}

	if (HttpServerConfigFacade::getInstance().isUseHttp2()) {
		HttpServerConfigFacade::getInstance().getHttp2StreamPool().expand(DEBUG_POOL_SIZE);
	}
#else
	static auto const maxUserConnection = HttpServerConfigFacade::getInstance().getMaxUserConnection();
	
	HttpServerConfigFacade::getInstance().getHttpRequestPool().expand(maxUserConnection);
	HttpServerConfigFacade::getInstance().getHttpResponsePool().expand(maxUserConnection);

	if (HttpServerConfigFacade::getInstance().isUseSSL()) {
		HttpServerConfigFacade::getInstance().getHttpsContextPool().expand(maxUserConnection);
	} else {
		HttpServerConfigFacade::getInstance().getHttpContextPool().expand(maxUserConnection);
	}

	if (HttpServerConfigFacade::getInstance().isUseHttp2()) {
		HttpServerConfigFacade::getInstance().getHttp2StreamPool().expand(maxUserConnection);
	}
#endif

	if (HttpServerConfigFacade::getInstance().isUseHttp2()) {
		RAPID_LOG_INFO() << "Enable HTTP/2";
	}

	server_.startListening([this](rapid::ConnectionPtr &pConn) {
		onNewConnection(pConn);
	}, HttpServerConfigFacade::getInstance().getNumaNode());
	RAPID_LOG_INFO() << "HttpServer starting...";
	RAPID_LOG_INFO() << "Press 'Ctrl+C' to stop";
	std::unique_lock<std::mutex> lock(waitStopMutex_);
	stopFlag_.wait(lock);
	server_.shutdown();
}

void HttpServer::onNewConnection(rapid::ConnectionPtr &pConn) {
	pConn->setAcceptEventHandler([this](rapid::ConnectionPtr &conn) {
		auto const &remoteAddress = conn->getRemoteSocketAddress();
		RAPID_LOG_INFO() << "Accepted address: " << remoteAddress.toString();
		auto pContext = insertHttpContext(remoteAddress.hash());
		if (HttpServerConfigFacade::getInstance().isUseSSL()) {
			conn->setSendEventHandler([pContext](rapid::ConnectionPtr &c) {
				pContext->handshake(c);
			});
			conn->setReceiveEventHandler([pContext](rapid::ConnectionPtr &c) {
				pContext->handshake(c);
			});
		}
		pContext->handshake(conn);
	});

	pConn->setDisconnectEventHandler([this](rapid::ConnectionPtr &conn) {
		auto const &remoteAddress = conn->getRemoteSocketAddress();
		RAPID_LOG_INFO() << "Disconnect address: " << remoteAddress.toString();
		auto pContext = removeHttpContext(remoteAddress.hash());
		pContext->onDisconnect(conn);
	});
}

HttpContextPtr HttpServer::createHttpContext() {
	RAPID_TRACE_CALL();

	if (HttpServerConfigFacade::getInstance().isUseSSL())
		return HttpContextPtr(HttpServerConfigFacade::getInstance().getHttpsContextPool().borrowObject());
	return HttpContextPtr(HttpServerConfigFacade::getInstance().getHttpContextPool().borrowObject());
}

HttpContextPtr HttpServer::removeHttpContext(size_t id) {
	RAPID_TRACE_CALL();

	std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
	auto itr = contextMap_.find(id);
	RAPID_ENSURE(itr != contextMap_.end());
	auto pContext = (*itr).second;
	contextMap_.erase(itr);
	return pContext;
}

HttpContextPtr HttpServer::insertHttpContext(size_t id) {
	auto pContext = createHttpContext();
	std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
	auto ret = contextMap_.insert(std::make_pair(id, pContext));
	RAPID_ENSURE(ret.second);
	return pContext;
}