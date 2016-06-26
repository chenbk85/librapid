//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

#include "../messagedispatcher.h"
#include "../httpcodec.h"
#include "websocketconstants.h"

enum WebSocketOpcodes : uint8_t {
	WS_OP_CONTINUATION = 0x00,
	WS_OP_TEXT = 0x01,
	WS_OP_BINARY = 0x02,
	WS_OP_CLOSED = 0x08,
	WS_OP_PING = 0x09,
	WS_OP_PONG = 0x0A,
};

std::string getWebsocketAcceptKey(std::string const & key);

class WebSocketRequest : public MessageContext {
public:
	WebSocketRequest();
	
	virtual ~WebSocketRequest() noexcept;
	
	bool isClosed() const noexcept;

	void setOpcode(WebSocketOpcodes opCode) noexcept;

	bool isTextFormat() const noexcept;

	bool isBinaryFormat() const noexcept;

	bool isPing() const noexcept;

	bool isPong() const noexcept;

	void setContentLength(uint64_t contentLength) noexcept;

	uint64_t contentLength() const noexcept;

protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;

private:
	WebSocketOpcodes opcode_;
	uint64_t contentLength_;
};

class WebSocketResponse : public MessageContext {
public:
	WebSocketResponse();
	
	virtual ~WebSocketResponse();

	void setCodec(WebSocketOpcodes opcodec);

	void setContentLength(uint64_t length);
protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;
	WebSocketOpcodes opcodec_;
	char mask_[WS_MASK_SIZE];
	uint64_t contentLength_;
};

class WebSocketFrameReader {
public:
	WebSocketFrameReader();

	WebSocketFrameReader(WebSocketFrameReader const &) = delete;
	WebSocketFrameReader& operator=(WebSocketFrameReader const &) = delete;
	
	uint64_t conetentLength() const noexcept;

	uint32_t readFrame(rapid::IoBuffer* buffer, uint32_t &wantReadSize);

	void reset() noexcept;

	WebSocketOpcodes opcode() const noexcept;

private:
	enum State {
		WS_PARSE_FIN,
		WS_PARSE_EXPECTED_SIZE,
		WS_PARSE_READ_DATA,
		WS_PARSE_DONE,
	};

	void parseFinAndContentLength(rapid::IoBuffer* buffer);

	bool isReadyToParseLength(rapid::IoBuffer* buffer);

	void parseContentLength(rapid::IoBuffer* buffer);

	void decodeData(rapid::IoBuffer *buffer);

	uint32_t getMaskSize() const noexcept;

	bool isFinFrame_ : 1;
	bool isMasked_ : 1;
	uint8_t unparsedContentLength_;
	WebSocketOpcodes opcode_;
	State state_;
	char mask_[WS_MASK_SIZE];
	uint32_t lengthAndMaskSize_;
	uint64_t contentLength_;
};

class WebSocketCodec : public HttpCodec {
public:
	explicit WebSocketCodec(std::shared_ptr<MessageDispatcher<WebSocketRequest>> disp);

	virtual ~WebSocketCodec() = default;

	WebSocketCodec(WebSocketCodec const &) = delete;
	WebSocketCodec& operator=(WebSocketCodec const &) = delete;

	virtual void readLoop(rapid::ConnectionPtr &pConn, uint32_t &bytesToRead) override;
private:
	std::shared_ptr<MessageDispatcher<WebSocketRequest>> dispatcher_;
	WebSocketFrameReader reader_;
	std::shared_ptr<WebSocketRequest> pWebSocketRequest_;
};
