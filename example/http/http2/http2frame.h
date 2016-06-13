//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>

#include <rapid/iobuffer.h>
#include "http2constants.h"

class Http2Frame;

class MalformedHttp2FrameException : public std::exception {
public:
	explicit MalformedHttp2FrameException(Http2Error err)
		: std::exception("Malformed http2 frame")
		, error(err) {
	}

	Http2Error error;
};

class Http2Settings {
public:
	Http2Settings();

	void reset();

	void set(uint16_t id, uint32_t value);

	void remove(uint16_t id);

	bool has(Http2SettingFlag flag) const {
		return settings_[static_cast<uint16_t>(flag)] != -1;
	}

	void fromBuffer(Http2Frame const &frame, rapid::IoBuffer* buffer);

	void toBuffer(rapid::IoBuffer* buffer);

private:
	uint32_t settings_[_H2_SETTINGS_MAX_];
};

class Http2Frame {
public:
	Http2Frame()
		: type(_H2_FRAME_TYPE_MAX_)
		, flags(_H2_FLAG_MAX_)
		, streamId(0)
		, contentLength(0) {
	}

	void reset() noexcept {
		type = _H2_FRAME_TYPE_MAX_;
		flags = _H2_FLAG_MAX_;
		streamId = 0;
		contentLength = 0;
	}

	bool hasFlag(uint8_t flag) const noexcept {
		return (flags & flag) != 0;
	}

	bool isServerID() const noexcept {
		return streamId % 2 == 0;
	}

	friend std::ostream& operator<<(std::ostream &ostr, Http2Frame const &frame);

	Http2FreamType type;
	uint8_t flags;
	uint32_t streamId;
	uint32_t contentLength;
};

class Http2FrameReader {
public:
	enum State {
		DONE = 0,
		CONNECTION_PREFACE,
		FRAME_HEADER,
		READ_PAYLOAD,
	};

	Http2FrameReader();

	void readFrame(rapid::IoBuffer* buffer, Http2Frame &frame, uint32_t &wantReadSize);

	void reset() noexcept;

private:
	State state_;
};

void writeHttp2Frame(rapid::IoBuffer *pBuffer, Http2Frame &frame, char *pFrameStart);

void writeWindowUpdateFrame(rapid::IoBuffer *pBuffer, uint32_t streamID, uint32_t windowSize);

void writeSettingAckFrame(rapid::IoBuffer *pBuffer);

void writePingAckFrame(rapid::IoBuffer *pBuffer, uint8_t const *buffer, uint32_t bufferLen);

void writeRstFrame(rapid::IoBuffer *pBuffer, uint32_t streamID, Http2Error error = H2_NO_ERROR);
