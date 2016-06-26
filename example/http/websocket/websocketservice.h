//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include "../httpcontext.h"
#include "websocketcodec.h"

class WebSocketService : public std::enable_shared_from_this<WebSocketService> {
public:
	WebSocketService();

	virtual ~WebSocketService() = default;

	static std::shared_ptr<WebSocketService> createService(std::string const &protocol);

	virtual void onOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) = 0;

	virtual bool onMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) = 0;

	virtual void onClose(rapid::ConnectionPtr &pConn) = 0;
};

