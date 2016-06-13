//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include "http2codec.h"
#include "streamdependency.h"

Http2StreamDependency::Http2StreamDependency() {
}

void Http2StreamDependency::add(std::shared_ptr<Http2Stream> const &st) {
	streams_[st->getStreamId()] = st;
}

bool Http2StreamDependency::get(uint32_t streamId, std::shared_ptr<Http2Stream> &st) {
	auto itr = streams_.find(streamId);

	if (itr != streams_.end()) {
		st = (*itr).second;
		return true;
	}
	return false;
}

void Http2StreamDependency::adjustPiority(uint32_t streamId, Http2StreamPriority const &priority) {
	auto st = streams_.find(streamId);

	RAPID_ENSURE(st != streams_.end());
	(*st).second->weight = priority.weight;
	
	auto parent = streams_.find(priority.streamDependency);
	if (st == parent) {
		return;
	}
	
	if (parent != streams_.end()) {
		for (auto node = (*parent).second; node != nullptr; node = node->parent){
			if (node == (*st).second) {
				(*parent).second->parent = (*st).second->parent;
				break;
			}
		}
		(*st).second->parent = (*parent).second;
	}

	if (priority.exclusive
		&& ((*st).second->parent != nullptr || (priority.streamDependency == 0))) {
		for (auto itr : streams_) {
			if ((itr.first != (*st).first) && (itr.second->parent == (*st).second->parent)) {
				itr.second->parent = (*st).second;
			}
		}
	}
}