//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <memory>

#include <MurmurHash3.h>

class HttpStaticHeaderTable {
public:
	HttpStaticHeaderTable();

	~HttpStaticHeaderTable();

	HttpStaticHeaderTable(HttpStaticHeaderTable const &) = delete;
	HttpStaticHeaderTable& operator=(HttpStaticHeaderTable const &) = delete;

	std::pair<uint32_t, std::string const*> getIndexedName(std::string const& name) const;

	std::pair<uint32_t, std::string const*> getIndexedName(uint32_t hashValue) const;

	static __forceinline uint32_t hash(std::string const& name) noexcept {
		uint32_t value = 0;
		MurmurHash3_x86_32(name.c_str(), static_cast<uint32_t>(name.length()), 0, &value);
		return value;
	}

private:
	static size_t constexpr HASH_TABLE_SIZE = 2048;
	std::vector<std::vector<std::pair<uint32_t, std::unique_ptr<std::string>>>> headerHashTable_;
};

