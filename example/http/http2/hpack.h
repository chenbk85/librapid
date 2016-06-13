//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <vector>
#include <map>
#include <stdexcept>
#include <deque>

#include <rapid/iobuffer.h>

std::string const H2_HEADER_AUTHORITY = { ":authority" };
std::string const H2_HEADER_METHOD = { ":method" };
std::string const H2_HEADER_PATH = { ":path" };
std::string const H2_HEADER_STATUS = { ":status" };

struct HpackEntry {
	HpackEntry();

	HpackEntry(char const *name, char const *value);

	size_t bytes() const throw();

	bool hasValue() const throw();

	bool operator==(HpackEntry const& other) const throw();

	bool operator<(HpackEntry const& other) const throw();

	bool operator>(HpackEntry const& other) const throw();

	void clear();

	bool isValid() const;
	
	std::string name_;
	std::string value_;
	bool noIndexing_{ false };
};

class IndexOutOfRangeException : public std::exception {
public:
	IndexOutOfRangeException()
		: std::exception("Index out of range") {
	}
};

class IndexTable {
public:
	IndexTable();

	void addHpackEntry(HpackEntry &entry);
	
	HpackEntry lookup(int index) const;
	
	void setHeaderTableSize(uint32_t tableSize);
	
	uint32_t getHeaderTableSize() const;

private:
	void evict();

	std::deque<HpackEntry> dynamicTable_;
	uint32_t currentSize_;
	uint32_t headerTableSize_;
};

class Http2Hpack {
public:
	Http2Hpack();
	
	static void encodeHeader(rapid::IoBuffer* buffer, std::string const &name, std::string const &value);
	
	std::map<std::string, std::string> decodeHeader(rapid::IoBuffer* buffer, uint32_t bufferLen);

private:
	void parseHeaderPair(rapid::IoBuffer* buffer, uint8_t index, HpackEntry &entry);
	
	void parseHeaderUpdate(rapid::IoBuffer* buffer, uint8_t firstByte);
	
	void parseIndexedHeaderFiled(rapid::IoBuffer* buffer, HpackEntry &entry);
	
	IndexTable indexTable_;
};

