//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <rapid/connection.h>

class HttpCodec {
public:
	virtual ~HttpCodec() = default;

	HttpCodec(HttpCodec const &) = delete;
	HttpCodec& operator=(HttpCodec const &) = delete;

	virtual void readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) = 0;
protected:
	HttpCodec() = default;
};

