//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <mutex>

#include <rapid/platform/spinlock.h>
#include "openssl/sslcontext.h"

#include "httpmessage.h"
#include "httpcodec.h"

#include "predeclare.h"

class HttpContext {
public:
	HttpContext();

	HttpContext(HttpContext const &) = delete;
	HttpContext& operator=(HttpContext const &) = delete;

	virtual ~HttpContext();

	virtual void handshake(rapid::ConnectionPtr &pConn);

	virtual void readLoop(rapid::ConnectionPtr &pConn);

	virtual void sendMessage(rapid::ConnectionPtr &pConn);

	virtual void onDisconnect(rapid::ConnectionPtr &pConn);

	void setId(size_t id) noexcept;

	size_t id() const noexcept;

protected:
	void onAcceptConnection(rapid::ConnectionPtr &pConn);

	void readPostData(rapid::ConnectionPtr &pConn);

	// Switch to protocol handler

	void sendSwitchToHttp2cMessage(rapid::ConnectionPtr &pConn);

	void sendSwitchToWebSocketMessage(rapid::ConnectionPtr &pConn);

	// HTTP message handler

	void onHttpGetMessage(rapid::ConnectionPtr &pConn);

	void onHttpPostMessage(rapid::ConnectionPtr &pConn);

	// WebSocket message handler

	void onWebSocketMessage(rapid::ConnectionPtr &pConn);

private:
	void setEventHandler();

protected:
	volatile bool hasUpgraded_ : 1;
	size_t id_;
	std::unique_ptr<HttpCodec> pHttpCodec_;
	WebSocketServicePtr pWebSocketService_;
	WebSocketRequestPtr pWebSocketRequest_;
	HttpRequestPtr pHttpRequest_;
	HttpResponsePtr pHttpResponse_;
};
