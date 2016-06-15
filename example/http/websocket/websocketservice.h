#pragma once

#include <string>

#include <rapid/connection.h>
#include <rapid/details/timingwheel.h>

#include "../httpcontext.h"
#include "websocketcodec.h"

class WebSocketService : public std::enable_shared_from_this<WebSocketService> {
public:
	WebSocketService();

	virtual ~WebSocketService() = default;

	static std::shared_ptr<WebSocketService> createService(std::string const &protocol);

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) = 0;

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) = 0;

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) = 0;
};

