#pragma once

#include <string>

#include <rapid/connection.h>
#include <rapid/details/timingwheel.h>

#include "../httpcontext.h"
#include "websocketcodec.h"

class WebSocketService {
public:
	WebSocketService();

	virtual ~WebSocketService() = default;

	static std::shared_ptr<WebSocketService> createService(std::string const &protocol);

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn) = 0;

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) = 0;

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) = 0;
private:
	static std::shared_ptr<WebSocketService> createPerfmonService();
	static std::shared_ptr<WebSocketService> createChatService();
};

