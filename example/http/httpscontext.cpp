//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include "httpserverconfigfacade.h"
#include "openssl/sslmanager.h"
#include "httpscontext.h"

HttpsContext::HttpsContext()
	: selectedALPN_(ALPN_HTTP_V1_1)
	, engine_(*SSLManager::getInstance().getDefaultSSLContext()) {
	engine_.reset();
}

HttpsContext::~HttpsContext() {

}

void HttpsContext::handshake(rapid::ConnectionPtr &pConn) {
	auto pSource = pConn->getReceiveBuffer();
	auto pDest = pConn->getSendBuffer();

	do {
		if (engine_.handshake(pSource, pDest)) {
			selectedALPN_ = engine_.getALPN();
			if (selectedALPN_ == APLNProtocols::ALPN_HTTP_V2) {
				hasUpgraded_ = true; // Upgrade HTTP2 from HTTPs
			}
			HttpContext::handshake(pConn);
			break;
		}
		if (!pDest->isEmpty()) {
			if (!pDest->send(pConn)) break;
		}
	} while (pSource->readSome(pConn));
}

void HttpsContext::readLoop(rapid::ConnectionPtr &pConn) {
	uint32_t bytesToRead = 0;
	auto pBuffer = pConn->getReceiveBuffer();

	do {
		if (!pBuffer->isEmpty()) {
			if (engine_.decrypt(pBuffer)) {
				pHttpCodec_->readLoop(pConn, bytesToRead);
				if (!bytesToRead) break; // Decode done!
			}
		}
	} while (pBuffer->readSome(pConn));
}

void HttpsContext::sendMessage(rapid::ConnectionPtr &pConn) {
	auto pBuffer = pConn->getSendBuffer();

	while (!pHttpResponse_->send(pConn, pHttpRequest_)) {
		engine_.encrypt(pBuffer);
		if (!pConn->sendAsync()) {
			return;
		}
	}

	pHttpResponse_->reset();
	pHttpRequest_->removeAll();
	readLoop(pConn);
}

void HttpsContext::onDisconnect(rapid::ConnectionPtr &pConn) {
	engine_.reset();
	HttpContext::onDisconnect(pConn);
}