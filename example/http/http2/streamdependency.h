//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <unordered_map>

#include <rapid/details/contracts.h>

class Http2Stream;
class Http2StreamPriority;

class Http2StreamDependency {
public:
	Http2StreamDependency();

	void add(std::shared_ptr<Http2Stream> const &st);

	bool get(uint32_t streamId, std::shared_ptr<Http2Stream> &st);

	void adjustPiority(uint32_t streamId, Http2StreamPriority const &priority);

private:
	std::unordered_map<uint32_t, std::shared_ptr<Http2Stream>> streams_;
};
