//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include "openssl/sslcontext.h"
#include "httpcontext.h"

class HttpsContext final : public HttpContext {
public:
	HttpsContext();

	virtual ~HttpsContext();

	virtual void handshake(rapid::ConnectionPtr &pConn) override;

	virtual void readLoop(rapid::ConnectionPtr &pConn) override;

	virtual void sendMessage(rapid::ConnectionPtr &pConn) override;

	virtual void onDisconnect(rapid::ConnectionPtr &pConn) override;
private:
	APLNProtocols selectedALPN_ : 1;
	OpenSslEngine engine_;
};
