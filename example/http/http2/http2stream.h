//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>

#include "http2constants.h"

class Http2Frame;

class Http2StreamPriority {
public:
	Http2StreamPriority()
		: exclusive(false)
		, weight(0)
		, streamDependency(0) {
	}

	bool exclusive : 1;
	uint32_t weight;
	uint32_t streamDependency;
};

class Http2Stream {
public:
	Http2Stream()
		: isSending(false)
		, isReceiving(false)
		, closedByUs(false)
		, closedWithRst(false)
		, streamId(0)
		, frameType(_H2_FRAME_TYPE_MAX_)
		, state(H2_STREAM_STATE_IDLE)
		, weight(0)
		, streamDependency(0)
		, windowSize(H2_DEFAULT_WINDOW_SIZE) {
	}

	void reset() noexcept;

	uint32_t getStreamId() const noexcept;

	void setStreamId(uint32_t id) noexcept;

	bool operator<(Http2Stream const &stream) const noexcept {
		return weight < stream.weight;
	}

	bool operator>(Http2Stream const &stream) const noexcept {
		return weight > stream.weight;
	}

	bool operator==(Http2Stream const &stream) const noexcept {
		return streamId == stream.streamId;
	}

	void updateState(Http2Frame const &frame);

	std::shared_ptr<Http2Stream> parent;
	bool isSending : 1;
	bool isReceiving : 1;
	bool closedByUs : 1;
	bool closedWithRst : 1;
	uint32_t streamId;
	uint8_t frameType;
	HttpStreamState state;
	uint32_t weight;
	uint32_t streamDependency;
	uint32_t windowSize;
};
