//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/utils/singleton.h>

#include "httpserverconfigfacade.h"

#include "httpheaders.h"

HttpHeaderName::HttpHeaderName(char const *name)
	: name_(name) {	
	hash_ = HttpStaticHeaderTable::hash(name);
}

HttpHeaderName::HttpHeaderName(std::string &&name)
	: name_(std::forward<std::string>(name)) {
	hash_ = HttpStaticHeaderTable::hash(name);
}

HttpHeaders::HttpHeaders() {
	headerNames_.reserve(HTTP_HEADER_MAX);
	headerValues_.reserve(HTTP_HEADER_MAX);
}

HttpHeaders::~HttpHeaders() {	
}

void HttpHeaders::add(std::string const &name, char const *value, uint32_t valueLength) {
	auto indexedName = HttpServerConfigFacade::getInstance().getHeadersTable().getIndexedName(name);
	if (!indexedName.second) {
		return;
	}
	headerNames_.push_back(indexedName);
	headerValues_.emplace_back(value, valueLength);
}

std::string const& HttpHeaders::get(HttpHeaderName const &header) const {
	static std::string const EMPTY_STRING;

	uint32_t numHeaderNames = static_cast<uint32_t>(headerNames_.size());
	for (uint32_t i = 0; i < numHeaderNames; ++i) {
		if (header.hash() == headerNames_[i].first) {
			return headerValues_[i];
		}
	}
	return EMPTY_STRING;
}

void HttpHeaders::add(HttpHeaderName const &header, std::string const &value) {
	headerNames_.push_back(std::make_pair(header.hash(), &header.str()));
	headerValues_.emplace_back(value.c_str(), value.length());
}

void HttpHeaders::add(HttpHeaderName const& name, int64_t value) {
	add(name, std::to_string(value));
}

void HttpHeaders::remove(HttpHeaderName const& header) {
	uint32_t numHeaderNames = static_cast<uint32_t>(headerNames_.size());
	for (uint32_t i = 0; i < numHeaderNames; ++i) {
		if (header.hash() == headerNames_[i].first) {
			headerNames_.erase(std::begin(headerNames_) + i);
			headerValues_.erase(std::begin(headerValues_) + i);
			break;
		}
	}
}

void HttpHeaders::clear() {
	headerNames_.clear();
	headerValues_.clear();
}

