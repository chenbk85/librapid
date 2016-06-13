//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/utilis.h>
#include <rapid/logging/logging.h>

#include "http2frame.h"

static void writeHttp2Frame(rapid::IoBuffer* buffer, Http2Frame const &frame) {
	rapid::writeData<rapid::utils::BigEndian>(buffer, reinterpret_cast<uint8_t const *>(&frame.contentLength), 3);
	rapid::writeData(buffer, static_cast<uint8_t>(frame.type));
	rapid::writeData(buffer, static_cast<uint8_t>(frame.flags));
	rapid::writeData<rapid::utils::BigEndian>(buffer, frame.streamId);
}

void writeHttp2Frame(rapid::IoBuffer *pBuffer, Http2Frame &frame, char *pFrameStart) {
	frame.contentLength = pBuffer->readable() - H2_FRAME_SIZE;
	RAPID_ENSURE(frame.contentLength > 0);
	rapid::utils::swapBytes(&pFrameStart, reinterpret_cast<char const *>(&frame.contentLength), 3);
	*pFrameStart++ = frame.type;
	*pFrameStart++ = frame.flags;
	rapid::utils::swapBytes(&pFrameStart, reinterpret_cast<char const *>(&frame.streamId), 4);
}

void writeWindowUpdateFrame(rapid::IoBuffer *pBuffer, uint32_t streamID, uint32_t windowSize) {
	Http2Frame windowSizeFrame;
	windowSizeFrame.type = H2_FRAME_WINDOW_UPDATE;
	windowSizeFrame.flags = H2_FLAG_EMPTY;
	windowSizeFrame.streamId = streamID;
	windowSizeFrame.contentLength = 4;
	writeHttp2Frame(pBuffer, windowSizeFrame);
	rapid::writeData<rapid::utils::BigEndian>(pBuffer, windowSize);
}

void writeSettingAckFrame(rapid::IoBuffer *pBuffer) {
	Http2Frame settingAckFrame;
	settingAckFrame.type = H2_FRAME_SETTINGS;
	settingAckFrame.flags = H2_FLAG_SETTINGS_ACK;
	settingAckFrame.streamId = 0;
	settingAckFrame.contentLength = 0;
	writeHttp2Frame(pBuffer, settingAckFrame);
}

void writePingAckFrame(rapid::IoBuffer *pBuffer, uint8_t const *buffer, uint32_t bufferLen) {
	Http2Frame pingAckFrame;
	pingAckFrame.type = H2_FRAME_PING;
	pingAckFrame.flags = H2_FLAG_PING_ACK;
	pingAckFrame.streamId = 0;
	pingAckFrame.contentLength = bufferLen;
	writeHttp2Frame(pBuffer, pingAckFrame);
	rapid::writeData<rapid::utils::BigEndian>(pBuffer, buffer, bufferLen);
	RAPID_LOG_TRACE() << pingAckFrame;
}

void writeRstFrame(rapid::IoBuffer *pBuffer, uint32_t streamID, Http2Error error) {
	Http2Frame rstFrame;
	rstFrame.type = H2_FRAME_RST_STREAM;
	rstFrame.streamId = streamID;
	rstFrame.flags = H2_FLAG_EMPTY;
	rstFrame.contentLength = 4;
	writeHttp2Frame(pBuffer, rstFrame);
	rapid::writeData<rapid::utils::BigEndian>(pBuffer, error);
	RAPID_LOG_TRACE() << rstFrame;
}

std::ostream& operator << (std::ostream &ostr, Http2Frame const &frame) {
	static char const * fameTypeStringTable[] {
		{ "DATA" },
		{ "HEADERS" },
		{ "PRIORITY" },
		{ "RST" },
		{ "SETTINGS" },
		{ "PUSH_PROMISE" },
		{ "PING" },
		{ "GOAWAY" },
		{ "WINDOW_UPDATE" },
		{ "CONTINUATION" }
	};

	ostr << fameTypeStringTable[frame.type] << " frame"
		<< " (stream_id=" << frame.streamId << ","
		<< " length=" << frame.contentLength << ","
		<< " flag=" << static_cast<uint32_t>(frame.flags)
		<< ")";
	return ostr;
}

Http2Settings::Http2Settings() {
	reset();
}

void Http2Settings::reset() {
	memset(settings_, -1, _H2_SETTINGS_MAX_);
}

void Http2Settings::remove(uint16_t id) {
	RAPID_ENSURE(id < _H2_SETTINGS_MAX_);
	settings_[id] = -1;
}

void Http2Settings::set(uint16_t id, uint32_t value) {
	auto flags = static_cast<Http2SettingFlag>(id);

	switch (flags) {
	case H2_SETTINGS_ENABLE_PUSH:
		RAPID_LOG_INFO() << "Enable push: " << (value ? "true" : "false");
		break;
	case H2_SETTINGS_HEADER_TABLE_SIZE:
		RAPID_LOG_INFO() << "Set header table size: " << value;
		break;
	case H2_SETTINGS_MAX_CONCURRENT_STREAMS:
		RAPID_LOG_INFO() << "Set max concurrent streams: " << value;
		break;
	case H2_SETTINGS_MAX_INITIAL_WINDOW_SIZE:
		RAPID_LOG_INFO() << "Set max initial window size: " << value;
		break;
	case H2_SETTINGS_MAX_FRAME_SIZE:
		RAPID_LOG_INFO() << "Set max frame size: " << value;
		break;
	case H2_SETTINGS_MAX_HEADER_LIST_SIZE:
		RAPID_LOG_INFO() << "Set header list size: " << value;
		break;
	case H2_SETTINGS_EMPTY: break;
	case _H2_SETTINGS_MAX_: break;
	default: break;
	}

	RAPID_ENSURE(flags < _H2_SETTINGS_MAX_);
	settings_[id] = value;
}

void Http2Settings::toBuffer(rapid::IoBuffer* buffer) {
	uint16_t id = 0;

	for (auto value : settings_) {
		if (value != -1) {
			rapid::writeData<rapid::utils::BigEndian>(buffer, id);
			rapid::writeData<rapid::utils::BigEndian>(buffer, value);
		}
		++id;
	}
}


void Http2Settings::fromBuffer(Http2Frame const& frame, rapid::IoBuffer* buffer) {
	for (uint32_t i = 0; i < frame.contentLength; i += H2_FRAME_SETTINGS_PLAYLOAD_SIZE) {
		auto id = rapid::readUint16Big(buffer);
		auto value = rapid::readUint32Big(buffer);
		set(id, value);
	}
}

Http2FrameReader::Http2FrameReader()
	: state_(CONNECTION_PREFACE) {
}

void Http2FrameReader::readFrame(rapid::IoBuffer* buffer, Http2Frame &frame, uint32_t &wantReadSize) {
	switch (state_) {
	case CONNECTION_PREFACE:
		if (buffer->readable() < H2_CONNECTION_PREFACE_SIZE) {
			wantReadSize = H2_CONNECTION_PREFACE_SIZE - buffer->readable();
			break;
		} else {
			auto check = memcmp(buffer->peek(), H2_CONNECTION_PREFACE, H2_CONNECTION_PREFACE_SIZE);
			buffer->retrieve(H2_CONNECTION_PREFACE_SIZE);
			state_ = FRAME_HEADER;
		}
	case DONE:
	case FRAME_HEADER:
		if (buffer->readable() < H2_FRAME_SIZE) {
			wantReadSize = H2_FRAME_SIZE - buffer->readable();
			break;
		} else {
			frame.contentLength = rapid::readUint24Big(buffer);
			frame.type = static_cast<Http2FreamType>(rapid::readUint8(buffer));
			frame.flags = static_cast<Http2Flags>(rapid::readUint8(buffer));
			frame.streamId = rapid::readUint32Big(buffer);
			state_ = READ_PAYLOAD;
		}
	case READ_PAYLOAD:
		if (frame.contentLength > buffer->readable()) {
			wantReadSize = frame.contentLength - buffer->readable();
			break;
		} else {
			state_ = DONE;
		}
		break;
	}
}
