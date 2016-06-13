//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

#include <picohttpparser.h>

#include <rapid/iobuffer.h>

#include "httpconstants.h"
#include "httpmessage.h"
#include "httpcodec.h"

class Http1xCodec : public HttpCodec {
public:
	explicit Http1xCodec(std::shared_ptr<MessageDispatcher<HttpRequest>> disp);

	virtual ~Http1xCodec() = default;

	Http1xCodec(Http1xCodec const &) = delete;
	Http1xCodec& operator=(Http1xCodec const &) = delete;

	virtual void readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) override;

private:
	void parse(rapid::IoBuffer *buffer, rapid::ConnectionPtr &pConn);

	struct phr_header headers_[HTTP_HEADER_MAX];
	std::shared_ptr<MessageDispatcher<HttpRequest>> dispatcher_;
	std::shared_ptr<HttpRequest> pHttpRequest_;
};