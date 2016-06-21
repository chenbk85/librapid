//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/utils/singleton.h>
#include <rapid/logging/logging.h>

#include "httpserverconfigfacade.h"

#include "http1xcodec.h"

Http1xCodec::Http1xCodec(std::shared_ptr<MessageDispatcher<HttpRequest>> disp)
	: dispatcher_(disp) {
	pHttpRequest_ = std::shared_ptr<HttpRequest>(HttpServerConfigFacade::getInstance().getHttpRequestPool().borrowObject());
}

void Http1xCodec::readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) {
	auto buffer = pConn->getReceiveBuffer();
	if (buffer->isEmpty()) {
		bytesToRead = buffer->goodSize();
		return;
	}
	parse(buffer, pConn);
}

void Http1xCodec::parse(rapid::IoBuffer* buffer, rapid::ConnectionPtr& pConn) {
	char const* method = nullptr;
	char const* path = nullptr;

	size_t numHeaders = HTTP_HEADER_MAX;
	size_t prevbufLen = 0;
	size_t methodLen = 0;
	size_t pathLen = 0;
	auto minorVersion = 0;
	
	auto const bytesRead = buffer->readable();
	memset(headers_, 0, sizeof(headers_));
	
	auto retval = ::phr_parse_request(buffer->peek(),
		bytesRead,
		&method,
		&methodLen,
		&path,
		&pathLen,
		&minorVersion,
		headers_,
		&numHeaders,
		prevbufLen);

	if (retval == -2) {
		// Request incomplete.
		RAPID_LOG_WARN_STACK_TRACE() << "Request incomplete!";
	} else if (retval > 0) {
		buffer->retrieve(bytesRead);

		// Successfully parsed the request.
		for (size_t i = 0; i < numHeaders; ++i) {
			std::string name(headers_[i].name, headers_[i].name_len);
			std::string value(headers_[i].value, headers_[i].value_len);
			pHttpRequest_->add(name, value);
		}

		pHttpRequest_->setVersion(minorVersion);
		pHttpRequest_->setMethod(std::string(method, methodLen));
		pHttpRequest_->setUri(path, pathLen);
		dispatcher_->onMessage(pHttpRequest_->method(), pConn, pHttpRequest_);
	} else {
		RAPID_LOG_WARN_STACK_TRACE() << "Parse error!";
		throw MalformedDataException();
	}
}
