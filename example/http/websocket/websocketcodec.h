//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <map>

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

protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;

private:
	WebSocketOpcodes opcode_;
};

class WebSocketResponse : public MessageContext {
public:
	WebSocketResponse();
	
	virtual ~WebSocketResponse();

	void setContentLength(uint64_t length);
protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;
	char mask_[WS_MASK_SIZE];
	uint64_t contentLength_;
};

class WebSocketFrameReader {
public:
	enum State {
		WS_PARSE_FIN,
		WS_PARSE_EXPECTED_SIZE,
		WS_PARSE_READ_DATA,
		WS_PARSE_DONE,
	};

	WebSocketFrameReader();
	
	uint64_t getConetentLength() const;

	void readFrame(rapid::IoBuffer* buffer, uint32_t &wantReadSize);

	void reset();

	WebSocketOpcodes opcode() const;

private:

	void parseFinAndContentLength(rapid::IoBuffer* buffer);

	bool isReadyRead(rapid::IoBuffer* buffer);

	void parseContentLength(rapid::IoBuffer* buffer);

	void maskData(rapid::IoBuffer *buffer);

	size_t getMaskSize() const;

	bool lastFrame_ : 1;
	bool isMasked_ : 1;
	uint8_t unparsedContentLength_;
	WebSocketOpcodes opcode_;
	State state_;
	char mask_[WS_MASK_SIZE];
	int indexOfFirstMask_;
	// 預期還要接收多少bytes才能夠接收到Length欄位和Mask欄位.
	uint64_t bytesReceiveLengthAndMask_;
	uint64_t contentLength_;
	uint64_t totalReadBytes_;
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
