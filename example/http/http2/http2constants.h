//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

// 未透過SETTINGS_MAX_FRAME_SIZE設置的最大值
static uint32_t const H2_DEFAULT_WINDOW_SIZE = 16384;

static uint32_t const H2_DEFAULT_SETTINGS_MAX_FRAME_SIZE = ((1 << 19) - 1);

static uint32_t const H2_PAYLOAD_LENGTH_MAX = 16777215UL;

static uint32_t const H2_FRAME_SIZE = 9;

static uint32_t const H2_PING_DATA_SIZE = 8;

static uint32_t const H2_WEIGHT_MAX = 256;

static char const H2_CONNECTION_PREFACE[] = { "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" };

static uint32_t const H2_CONNECTION_PREFACE_SIZE = 24;

static uint32_t const H2_GOWAY_FRAME_SIZE = 8;

static uint32_t const H2_PRIORITY_FRAME_SIZE = 5;

static uint32_t const H2_WINDOW_SIZE_UPDATE_PLAYLOAD_SIZE = 4;

static uint32_t const H2_FRAME_SETTINGS_PLAYLOAD_SIZE = 6;

enum Http2FreamType : uint8_t {
	H2_FRAME_DATA = 0,
	H2_FRAME_HEADERS,
	H2_FRAME_PRIORITY,
	H2_FRAME_RST_STREAM,
	H2_FRAME_SETTINGS,
	H2_FRAME_PUSH_PROMISE,
	H2_FRAME_PING,
	H2_FRAME_GOAWAY,
	H2_FRAME_WINDOW_UPDATE,
	H2_FRAME_CONTINUATION,
	_H2_FRAME_TYPE_MAX_
};

enum Http2Flags : uint8_t {
	H2_FLAG_EMPTY = 0x00,
	// Data frame
	H2_FLAG_DATA_FRAME_END_STREAM = 0x01,
	H2_FLAG_DATA_FRAME_PADDED = 0x08,
	// Headers frame
	H2_FLAG_HEADERS_END_STREAM = 0x01,
	H2_FLAG_HEADERS_END_HEADERS = 0x04,
	H2_FLAG_HEADERS_PADDED = 0x08,
	H2_FLAG_HEADERS_PRIORITY = 0x20,
	// Settings Frame
	H2_FLAG_SETTINGS_ACK = 0x01,
	// Ping Frame
	H2_FLAG_PING_ACK = 0x01,
	// Continuation Frame
	H2_FLAG_CONTINUATION_END_HEADERS = 0x04,
	// Push frame
	H2_FLAG_PUSH_PROMISE_END_HEADERS = 0x04,
	H2_FLAG_PUSH_PROMISE_PADDED = 0x08,
	_H2_FLAG_MAX_,
};

enum Http2SettingFlag : uint8_t {
	H2_SETTINGS_EMPTY = 0x00,
	H2_SETTINGS_HEADER_TABLE_SIZE = 0x01,
	H2_SETTINGS_ENABLE_PUSH,
	H2_SETTINGS_MAX_CONCURRENT_STREAMS,
	H2_SETTINGS_MAX_INITIAL_WINDOW_SIZE,
	H2_SETTINGS_MAX_FRAME_SIZE,
	H2_SETTINGS_MAX_HEADER_LIST_SIZE,
	_H2_SETTINGS_MAX_,
};

enum Http2Error : uint32_t {
	H2_NO_ERROR = 0,
	H2_PROTOCOL_ERROR = 1,
	H2_INTERNAL_ERROR = 2,
	H2_FLOW_CONTROL_ERROR = 3,
	H2_SETTINGS_TIMEOUT = 4,
	H2_STREAM_CLOSED = 5,
	H2_FRAME_SIZE_ERROR = 6,
	H2_REFUSED_STREAM = 7,
	H2_CANCEL = 8,
	H2_COMPRESSION_ERROR = 9,
	H2_CONNECT_ERROR = 10,
	H2_ENHANCE_YOUR_CALM = 11,
	H2_INADEQUATE_SECURITY = 12,
	H2_HTTP_1_1_REQUIRED = 13,
	_H2_ERROR_MAX_,
};

enum HttpStreamState : uint8_t {
	// 只接受Headers及PUSH_PROMISE兩種控制幀, 閒置狀態在送出或收到Headers後會改變為開啟的狀態(Open).
	// 若是送出或收到PUSH_PROMISE則改變狀態為保留狀態，在這狀態還分為本機及遠端.
	H2_STREAM_STATE_IDLE,
	H2_STREAM_STATE_RESERVED_LOCAL,
	H2_STREAM_STATE_RESERVED_REMOTE,
	H2_STREAM_STATE_OPEN,
	H2_STREAM_STATE_HALF_CLOSED_LOCAL,
	H2_STREAM_STATE_HALF_CLOSED_REMOTE,
	H2_STREAM_STATE_CLOSED_REMOTE,
	H2_STREAM_STATE_CLOSED,
	_H2_STREAM_STATE_MAX_,
};
