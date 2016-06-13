//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include <http_parser.h>
#include <rapid/details/contracts.h>

class Uri {
public:
	Uri()
		: valid_(false) {
	}

	explicit Uri(std::string const &str) {
		fromString(str);
	}

	void fromString(std::string const &str) {
		fromString(str.c_str(), str.length());
	}

	void fromString(char const *str, size_t length) {
		if (length > buffer_.size()) {
			buffer_.resize(length + 1);
		}
		memset(buffer_.data(), 0, buffer_.size());
		memcpy(buffer_.data(), str, length);
		valid_ = http_parser_parse_url(buffer_.data(), length, 0, &parser_) == 0;
	}

	bool valid() const noexcept {
		return valid_;
	}

	std::string scheme() const {
		return getField(UF_SCHEMA);
	}

	std::string host() const {
		return getField(UF_HOST);
	}

	std::string port() const {
		return getField(UF_PORT);
	}

	std::string path() const {
		return getField(UF_PATH);
	}

	std::string query() const {
		return getField(UF_QUERY);
	}

	std::string fragment() const {
		return getField(UF_FRAGMENT);
	}
private:
	std::string getField(http_parser_url_fields fields) const {
		RAPID_ENSURE(valid());
		return std::string(&buffer_[parser_.field_data[fields].off], parser_.field_data[fields].len);
	}

	bool valid_ : 1;
	http_parser_url parser_;
	std::vector<char> buffer_;
};
