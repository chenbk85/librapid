//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include <rapid/connection.h>

#include "../http1xcodec.h"
#include "../httpcodec.h"
#include "../httpmessage.h"

#include "searchpriorityqueue.h"
#include "streamdependency.h"
#include "http2constants.h"
#include "http2stream.h"
#include "http2frame.h"
#include "hpack.h"

class Http2Response : public HttpResponse {
public:
	Http2Response();

	virtual ~Http2Response() = default;

	virtual bool send(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpRequest> httpRequest) override;

	virtual bool writeContent(rapid::IoBuffer *pBuffer) override;

protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;

private:
	void writeDataFrame(rapid::IoBuffer *pBuffer);

	void writeHeadersFrame(rapid::IoBuffer *pBuffer);

	std::shared_ptr<Http2Stream> stream_;
	Http2Frame frame_;
	uint8_t paddingLen_;
	uint32_t dataFramePaddingLength_;
	uint32_t sendCount_;
};

class Http2Request : public HttpRequest {
public:
	struct Http2StreamCompare {
		bool operator()(std::shared_ptr<Http2Stream> const &left, std::shared_ptr<Http2Stream> const &right) const {
			return *left > *right;
		}
	};

	using PriorityQueue = SearchPriorityQueue<std::shared_ptr<Http2Stream>,
		std::vector<std::shared_ptr<Http2Stream>>,
		Http2StreamCompare>;

	virtual ~Http2Request() = default;

	Http2Request();

	std::map<uint32_t, std::shared_ptr<HttpRequest>> headerTable_;
	uint8_t pingData_[H2_PING_DATA_SIZE];
	PriorityQueue priorityQueue_;

protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;
};

class Http2Codec : public HttpCodec {
public:
	explicit Http2Codec(std::shared_ptr<MessageDispatcher<HttpRequest>> disp);

	Http2Codec(Http2Codec const &) = delete;
	Http2Codec& operator=(Http2Codec const &) = delete;

	virtual ~Http2Codec();

	virtual void readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) override;

private:
	bool parse(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseFrameSettings(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseHeaders(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseWindowSizeUpdate(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseData(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseGoaway(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseRstStream(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parsePing(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parsePushPromise(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parseContinution(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	bool parsePriority(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	void parseHeadersFragment(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	void addRequestHeader(std::map<std::string, std::string> const &headerTable, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame);

	void sendPendingResponse(rapid::ConnectionPtr& pConn);

	std::shared_ptr<Http2Request> defaultHttpRequest();

	std::shared_ptr<Http2Request> pHttpRequests_;
	Http2FrameReader reader_;
	std::shared_ptr<MessageDispatcher<HttpRequest>> dispatcher_;
	Http2StreamDependency streamDependency_;
	Http2Hpack hpack_;
	Http2Frame lastFrame_;
	Http2Settings settings_;
};
