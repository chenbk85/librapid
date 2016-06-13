//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <iomanip>

#include <rapid/iobuffer.h>
#include <rapid/utils/byteorder.h>
#include <rapid/utilis.h>
#include <rapid/utils/singleton.h>
#include <rapid/utils/stringutilis.h>
#include <rapid/logging/logging.h>

#include "../httpserverconfigfacade.h"
#include "../httpconstants.h"
#include "http2codec.h"

static std::ostream& operator<<(std::ostream &ostr, Http2Error error) {
	switch (error) {
	case H2_PROTOCOL_ERROR:
		return ostr << "Protocol error";
		break;
	case H2_INTERNAL_ERROR:
		return ostr << "Internal error";
		break;
	case H2_FLOW_CONTROL_ERROR:
		return ostr << "Flow control error";
		break;
	case H2_SETTINGS_TIMEOUT:
		return ostr << "Settings timeout error";
		break;
	case H2_STREAM_CLOSED:
		return ostr << "Stream closed error";
		break;
	case H2_FRAME_SIZE_ERROR:
		return ostr << "Fream size error";
		break;
	case H2_REFUSED_STREAM:
		return ostr << "Refused stream error";
		break;
	case H2_CANCEL:
		return ostr << "Cancel error";
		break;
	case H2_COMPRESSION_ERROR:
		return ostr << "Compression error";
		break;
	case H2_CONNECT_ERROR:
		return ostr << "Connect error";
		break;
	case H2_ENHANCE_YOUR_CALM:
		return ostr << "Enhance your calm error";
		break;
	case H2_INADEQUATE_SECURITY:
		return ostr << "Inadequate security error";
		break;
	case H2_HTTP_1_1_REQUIRED:
		return ostr << "Http 1.1 Required error";
		break;
	case H2_NO_ERROR:
		return ostr << "No error";
		break;
	default:
		RAPID_ENSURE(0 && "Unknown Http2 error");
		break;
	}
}

void Http2Stream::updateState(Http2Frame const &frame) {
	auto oldState = state;
	
	auto hasStreamError = false;
	auto hasProtocolError = false;

	switch (state) {
	case H2_STREAM_STATE_IDLE:
		if (frame.type == H2_FRAME_HEADERS) {
			state = H2_STREAM_STATE_OPEN;
			if (frame.hasFlag(H2_FLAG_HEADERS_END_STREAM)) {
				state = (isSending ? H2_STREAM_STATE_HALF_CLOSED_LOCAL : H2_STREAM_STATE_HALF_CLOSED_REMOTE);
			}
		} else if (isSending && frame.type == H2_FRAME_RST_STREAM) {
			state = H2_STREAM_STATE_OPEN;
		} else if (frame.type == H2_FRAME_PRIORITY
			|| frame.type == H2_FRAME_SETTINGS
			|| frame.type == H2_FRAME_WINDOW_UPDATE) {
			// No change
		} else {
			// TODO : handle PROTOCOL_ERROR
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_RESERVED_LOCAL:
		if (isSending && frame.type == H2_FRAME_HEADERS) {
			state = H2_STREAM_STATE_CLOSED_REMOTE;
		} else if (frame.hasFlag(H2_FRAME_RST_STREAM)) {
			state = H2_STREAM_STATE_CLOSED;
		} else if (frame.type == H2_FRAME_PRIORITY) {
			// No change
		} else {
			// TODO : handle PROTOCOL_ERROR
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_RESERVED_REMOTE:
		if (frame.hasFlag(H2_FRAME_RST_STREAM)) {
			state = (isSending ? H2_STREAM_STATE_HALF_CLOSED_LOCAL : H2_STREAM_STATE_HALF_CLOSED_REMOTE);
		} else if (isReceiving && frame.type == H2_FRAME_HEADERS) {
			state = H2_STREAM_STATE_HALF_CLOSED_LOCAL;
		} else if (frame.type == H2_FRAME_PRIORITY) {
			// No change
		} else {
			// TODO : handle PROTOCOL_ERROR
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_OPEN:
		if (frame.hasFlag(H2_FLAG_HEADERS_END_STREAM)) {
			state = (isSending ? H2_STREAM_STATE_HALF_CLOSED_LOCAL : H2_STREAM_STATE_HALF_CLOSED_REMOTE);
		} else if (frame.hasFlag(H2_FRAME_RST_STREAM)) {
			state = H2_STREAM_STATE_CLOSED;
		} else {
			// No change
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_HALF_CLOSED_LOCAL:
		if (frame.hasFlag(H2_FRAME_RST_STREAM)
			|| (isReceiving && frame.hasFlag(H2_FLAG_HEADERS_END_STREAM))) {
			state = H2_STREAM_STATE_CLOSED;
		} else if (isReceiving
			|| frame.type == H2_FRAME_PRIORITY
			|| (isSending && frame.type == H2_FRAME_WINDOW_UPDATE)) {
			// No change
		} else {
			// TODO : handle PROTOCOL_ERROR
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_CLOSED_REMOTE:
		if (frame.hasFlag(H2_FRAME_RST_STREAM)
			|| (isReceiving && frame.hasFlag(H2_FLAG_HEADERS_END_STREAM))) {
			state = H2_STREAM_STATE_CLOSED;
		} else if (isSending
			|| frame.type == H2_FRAME_PRIORITY
			|| (isReceiving && frame.type == H2_FRAME_WINDOW_UPDATE)) {
			// No change
		} else {
			// TODO : handle PROTOCOL_ERROR
			hasProtocolError = true;
		}
		break;
	case H2_STREAM_STATE_CLOSED:
		if (frame.type == H2_FRAME_PRIORITY
			|| (isSending && frame.type == H2_FRAME_RST_STREAM)) {
		} else {
			// TODO : handle STREAM_CLOSED
			hasStreamError = true;
		}
		break;
	case H2_STREAM_STATE_HALF_CLOSED_REMOTE: break;
	case _H2_STREAM_STATE_MAX_: break;
	default: break;
	}

	if ((state == H2_STREAM_STATE_CLOSED)
		&& (oldState != H2_STREAM_STATE_CLOSED)) {
		closedByUs = isSending;
		closedWithRst = frame.type == H2_FRAME_RST_STREAM;
	}

	if (frame.type == H2_FRAME_PUSH_PROMISE
		&& !hasProtocolError && !hasStreamError) {
	}
}

void Http2FrameReader::reset() noexcept {
	state_ = FRAME_HEADER;
}

void Http2Stream::setStreamId(uint32_t id) noexcept {
	streamId = id;
}

uint32_t Http2Stream::getStreamId() const noexcept {
	return streamId;
}

void Http2Stream::reset() noexcept {
	isSending = false;
	isReceiving = false;
	closedByUs = false;
	state = H2_STREAM_STATE_IDLE;
	weight = 0;
	windowSize = H2_DEFAULT_WINDOW_SIZE;
}

Http2Request::Http2Request() {
}

Http2Response::Http2Response()
	: paddingLen_(0) {
	dataFramePaddingLength_ = 0;
	sendCount_ = 0;
}

void Http2Response::writeHeadersFrame(rapid::IoBuffer *pBuffer) {
	if (frame_.hasFlag(H2_FLAG_HEADERS_PADDED)) {
		rapid::writeData(pBuffer, paddingLen_);
	}
}

bool Http2Response::send(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpRequest> httpRequest) {
	auto pHttp2Request = std::dynamic_pointer_cast<Http2Request>(httpRequest);
	
	if (pHttp2Request->priorityQueue_.empty()) {
		return true;
	}
	
	stream_ = pHttp2Request->priorityQueue_.top();

	RAPID_LOG_TRACE() << "send stream:" << stream_->getStreamId() << " weight:" << stream_->weight;

	switch (stream_->frameType) {
	case H2_FRAME_PING:
		pHttp2Request->priorityQueue_.pop();
		writePingAckFrame(pConn->getSendBuffer(), pHttp2Request->pingData_, H2_PING_DATA_SIZE);
		break;
	case H2_FRAME_SETTINGS:
		pHttp2Request->priorityQueue_.pop();
		writeSettingAckFrame(pConn->getSendBuffer());
		break;
	default:
		if (HttpResponse::send(pConn, pHttp2Request->headerTable_[stream_->getStreamId()])) {
			pHttp2Request->priorityQueue_.pop();
			// HTTP/2 有多個request單一個response, 每一個request處理完畢後, 需要將狀態進行reset.
			if (!pHttp2Request->priorityQueue_.empty()) {
				reset();
			}
		}
		break;
	}
	return false;
}

bool Http2Response::writeContent(rapid::IoBuffer *pBuffer) {
	if (stream_->state == H2_STREAM_STATE_CLOSED) {
		RAPID_LOG_TRACE() << "Stream " << stream_->getStreamId() << " Closed";
		return true;
	}
	
	// Setup HTTP2 frame 
	frame_.type = H2_FRAME_DATA;
	frame_.streamId = stream_->getStreamId();

	auto pFrameStart = pBuffer->writeData();
	pBuffer->advanceWriteIndex(H2_FRAME_SIZE);
	writeDataFrame(pBuffer);

	auto done = HttpResponse::writeContent(pBuffer);
	
	if (done) {
		pBuffer->reset();

		if (stream_->state == H2_STREAM_STATE_HALF_CLOSED_REMOTE) {
			writeRstFrame(pBuffer, stream_->getStreamId());
		}
		stream_->state = H2_STREAM_STATE_CLOSED;
		return false;
	}

	if (sendCount_ == 0) {
		frame_.flags = H2_FLAG_DATA_FRAME_END_STREAM;
	} else {
		frame_.flags = H2_FLAG_EMPTY;
		--sendCount_;
	}
	
	writeHttp2Frame(pBuffer, frame_, pFrameStart);
	stream_->windowSize -= frame_.contentLength;

	RAPID_LOG_TRACE() << frame_ << " (win:" << stream_->windowSize << ")";
	
	if (!stream_->windowSize) {
		writeWindowUpdateFrame(pBuffer, stream_->getStreamId(), H2_DEFAULT_WINDOW_SIZE);
		stream_->windowSize = H2_DEFAULT_WINDOW_SIZE;
	}
	return false;
}

void Http2Response::writeDataFrame(rapid::IoBuffer *pBuffer) {
	if (dataFramePaddingLength_ > 0) {
		frame_.flags = H2_FLAG_DATA_FRAME_PADDED;
		rapid::writeData(pBuffer, dataFramePaddingLength_);
	}
}

void Http2Response::doSerialize(rapid::IoBuffer *pBuffer) {
	setBufferLength(H2_DEFAULT_WINDOW_SIZE);
	
	// Setup HTTP2 frame 
	frame_.flags = H2_FLAG_HEADERS_END_HEADERS;
	frame_.type = H2_FRAME_HEADERS;
	frame_.streamId = stream_->getStreamId();

	auto pFrameStart = pBuffer->writeData();
	pBuffer->advanceWriteIndex(H2_FRAME_SIZE);

	writeHeadersFrame(pBuffer);

	Http2Hpack::encodeHeader(pBuffer, H2_HEADER_STATUS, std::to_string(statusCode()));
	headers_.foreach([pBuffer](std::string const &name, std::string const &value) {
		Http2Hpack::encodeHeader(pBuffer, name, value);
	});

	sendCount_ = getContentLength() / getBufferLength();
	writeHttp2Frame(pBuffer, frame_, pFrameStart);
}

void Http2Request::doSerialize(rapid::IoBuffer* pBuffer) {
}

Http2Codec::Http2Codec(std::shared_ptr<MessageDispatcher<HttpRequest>> disp)
	: dispatcher_(disp) {
}

Http2Codec::~Http2Codec() {
}

void Http2Codec::readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) {
	auto pBuffer = pConn->getReceiveBuffer();	
	auto readDone = false;

	while (!readDone) {
		reader_.readFrame(pBuffer, lastFrame_, bytesToRead);

		if (bytesToRead > 0) {
			if (pHttpRequests_ != nullptr) {
				sendPendingResponse(pConn);
				bytesToRead = 0;
			}
			break;
		}

		if (pBuffer->isEmpty()) {
			return;
		}

		std::shared_ptr<Http2Stream> pStream;
		if (!streamDependency_.get(lastFrame_.streamId, pStream)) {
			pStream = HttpServerConfigFacade::getInstance().getHttp2StreamPool().borrowObject();
			pStream->reset();
			pStream->setStreamId(lastFrame_.streamId);
			streamDependency_.add(pStream);
		}
		
		RAPID_LOG_TRACE() << "recv " << lastFrame_;
		pStream->updateState(lastFrame_);
		readDone = parse(pBuffer, pStream, lastFrame_);
		lastFrame_.reset();
		reader_.reset();
	}
}

bool Http2Codec::parse(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	switch (lastFrame_.type) {
	case H2_FRAME_DATA: 
		return parseData(pBuffer, stream, frame);
	case H2_FRAME_HEADERS:
		return parseHeaders(pBuffer, stream, frame);
	case H2_FRAME_PRIORITY:
		return parsePriority(pBuffer, stream, frame);
	case H2_FRAME_RST_STREAM:
		return parseRstStream(pBuffer, stream, frame);
	case H2_FRAME_SETTINGS:
		return parseFrameSettings(pBuffer, stream, frame);
	case H2_FRAME_PUSH_PROMISE:
		return parsePushPromise(pBuffer, stream, frame);
	case H2_FRAME_PING:
		return parsePing(pBuffer, stream, frame);
	case H2_FRAME_GOAWAY:
		return parseGoaway(pBuffer, stream, frame);
	case H2_FRAME_WINDOW_UPDATE:
		return parseWindowSizeUpdate(pBuffer, stream, frame);
	case H2_FRAME_CONTINUATION:
		return parseContinution(pBuffer, stream, frame);
	case _H2_FRAME_TYPE_MAX_: break;
	default: break;
	}
	return false;
}

void Http2Codec::sendPendingResponse(rapid::ConnectionPtr& pConn) {
	dispatcher_->onMessage(HTTP_METHOD_GET, pConn, pHttpRequests_);
	pHttpRequests_.reset();
}

std::shared_ptr<Http2Request> Http2Codec::defaultHttpRequest() {
	if (!pHttpRequests_) {
		pHttpRequests_ = std::shared_ptr<Http2Request>(HttpServerConfigFacade::getInstance().getHttp2RequestPool().borrowObject());
	}
	return pHttpRequests_;
}

/*

+---------------------------------------------------------------+
|                        Error Code (32)                       |
+---------------------------------------------------------------+

*/
bool Http2Codec::parseRstStream(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	auto error = static_cast<Http2Error>(rapid::readUint32Big(pBuffer));
	RAPID_LOG_TRACE() << error;
	return true;
}

/*

+---------------------------------------------------------------+
|                                                              |
|                      Opaque Data (64)                        |
|                                                              |
+---------------------------------------------------------------+

*/
bool Http2Codec::parsePing(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	pBuffer->read(reinterpret_cast<char*>(defaultHttpRequest()->pingData_), H2_PING_DATA_SIZE);
	stream->setStreamId(frame.streamId);
	stream->frameType = frame.type;
	defaultHttpRequest()->priorityQueue_.push(stream);
	return false;
}

/*

+-------------------------------+
|       Identifier (16)        |
+-------------------------------+-------------------------------+
|                        Value (32)                            |
+---------------------------------------------------------------+

*/
bool Http2Codec::parseFrameSettings(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	if (frame.hasFlag(H2_FLAG_SETTINGS_ACK) && frame.contentLength > 0) {
		throw MalformedHttp2FrameException(H2_FRAME_SIZE_ERROR);
	}
	
	if (frame.streamId != 0) {
		throw MalformedHttp2FrameException(H2_PROTOCOL_ERROR);
	}
	
	if (frame.contentLength % H2_FRAME_SETTINGS_PLAYLOAD_SIZE != 0) {
		throw MalformedHttp2FrameException(H2_FRAME_SIZE_ERROR);
	}
	
	settings_.reset();
	settings_.fromBuffer(frame, pBuffer);

	stream->setStreamId(frame.streamId);
	stream->frameType = frame.type;

	defaultHttpRequest()->priorityQueue_.push(stream);

	return false;
}

/*

+---------------+
|Pad Length? (8)|
+-+-------------+-----------------------------------------------+
|R|                  Promised Stream ID (31)                   |
+-+-----------------------------+-------------------------------+
|                   Header Block Fragment (*)                ...
+---------------------------------------------------------------+
|                           Padding (*)                      ...
+---------------------------------------------------------------+

*/
bool Http2Codec::parsePushPromise(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	return true;
}

/*

+-+-------------------------------------------------------------+
|E|                  Stream Dependency (31)                    |
+-+-------------+-----------------------------------------------+
|   Weight (8)  |
+-+-------------+

*/
bool Http2Codec::parsePriority(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	auto value = rapid::readUint32Big(pBuffer);
	
	Http2StreamPriority priority;
	priority.streamDependency = value & 0x7F;
	priority.exclusive = (value & 0x80) > 0;
	priority.weight = rapid::readUint8(pBuffer);
	
	RAPID_LOG_TRACE() << "(dependency stream_id=" << priority.streamDependency << ","
		<< " exclusive=" << (priority.exclusive ? "true" : "false") << ","
		<< " weight =" << priority.weight << ")";

	stream->setStreamId(frame.streamId);
	stream->streamDependency = priority.streamDependency;

	streamDependency_.adjustPiority(stream->getStreamId(), priority);
	return false;
}

/*

+---------------------------------------------------------------+
|                   Header Block Fragment (*)                ...
+---------------------------------------------------------------+

*/
bool Http2Codec::parseContinution(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	parseHeadersFragment(pBuffer, stream, frame);
	return false;
}

bool Http2Codec::parseHeaders(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	uint8_t headerPaddingLen = 0;

	RAPID_ENSURE(pBuffer->readable() >= frame.contentLength);

	if ((frame.flags & H2_FLAG_HEADERS_PADDED) > 0) {
		headerPaddingLen = rapid::readUint8(pBuffer);
	}

	if ((frame.flags & H2_FLAG_HEADERS_PRIORITY) > 0) {
		parsePriority(pBuffer, stream, frame);
	}

	parseHeadersFragment(pBuffer, stream, frame);
	return false;
}

void Http2Codec::parseHeadersFragment(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	auto headerFragmentLen = (frame.flags & H2_FLAG_HEADERS_PRIORITY) > 0
		? frame.contentLength - H2_PRIORITY_FRAME_SIZE : frame.contentLength;

	auto headerTable = hpack_.decodeHeader(pBuffer, headerFragmentLen);
	addRequestHeader(headerTable, stream, frame);
}

void Http2Codec::addRequestHeader(std::map<std::string, std::string> const &headerTable, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	auto pHttpRequest = std::shared_ptr<HttpRequest>(HttpServerConfigFacade::getInstance().getHttpRequestPool().borrowObject());

	std::string method;

	for (auto & header : headerTable) {
		if (header.first == H2_HEADER_METHOD) {
			method = header.second;
			pHttpRequest->setMethod(header.second);
			continue;
		} else if (header.first == H2_HEADER_PATH) {
			pHttpRequest->setUri(header.second);
			continue;
		} else if (header.first == H2_HEADER_AUTHORITY) {
			pHttpRequest->add(HTTP_HOST, header.second);
			continue;
		}
		pHttpRequest->add(header.first, header.second);
	}

	stream->setStreamId(frame.streamId);
	stream->frameType = frame.type;

	defaultHttpRequest()->headerTable_[stream->getStreamId()] = pHttpRequest;
	defaultHttpRequest()->priorityQueue_.push(stream);
}

/*

+-+-------------------------------------------------------------+
|R|              Window Size Increment (31)                    |
+-+-------------------------------------------------------------+

*/
bool Http2Codec::parseWindowSizeUpdate(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	if (frame.contentLength != H2_WINDOW_SIZE_UPDATE_PLAYLOAD_SIZE) {
		throw MalformedHttp2FrameException(H2_FRAME_SIZE_ERROR);
	}

	stream->windowSize = rapid::readUint32Big(pBuffer);
	RAPID_LOG_TRACE() << "Window size update: " << stream->windowSize;
	return false;
}

/*

+---------------+
|Pad Length? (8)|
+---------------+-----------------------------------------------+
|                            Data (*)                        ...
+---------------------------------------------------------------+
|                           Padding (*)                      ...
+---------------------------------------------------------------+

*/
bool Http2Codec::parseData(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	/*
	uint8_t dataPaddingLen = 0;
	if (frame.hasFlag(H2_FLAG_DATA_FRAME_PADDED)) {
		dataPaddingLen = rapid::readUint8(buffer);
	}

	auto pHttp2Request = std::shared_ptr<Http2Request>(HttpServerConfigFacade::getInstance().getHttp2RequestPool().borrowObject());
	stream->setStreamId(frame.streamId);
	pHttp2Request->priorityQueue_.push(stream);
	dispatcher_->onMessage(HTTP_METHOD_POST, pConn, pHttp2Request);
	*/
	return false;
}

/*

+-+-------------------------------------------------------------+
|R|                  Last-Stream-ID (31)                       |
+-+-------------------------------------------------------------+
|                      Error Code (32)                         |
+---------------------------------------------------------------+
|                  Additional Debug Data (*)                   |
+---------------------------------------------------------------+

*/
bool Http2Codec::parseGoaway(rapid::IoBuffer *pBuffer, std::shared_ptr<Http2Stream> &stream, Http2Frame const &frame) {
	auto lastStreamID = rapid::readUint32Big(pBuffer);

	auto error = static_cast<Http2Error>(rapid::readUint32Big(pBuffer));
	
	RAPID_LOG_TRACE() << error;

	auto debugDataLen = frame.contentLength - H2_GOWAY_FRAME_SIZE;
	if (!debugDataLen) {
		RAPID_LOG_ERROR() << "Empty debug data!";
		return false;
	}

	std::vector<char> temp(debugDataLen + 1);
	pBuffer->read(temp.data(), debugDataLen);
	RAPID_LOG_ERROR() << temp.data();
	return false;
}
