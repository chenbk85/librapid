//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <unordered_map>
#include <iomanip>

#include <rapid/logging/logging.h>
#include <rapid/utils/stringutilis.h>
#include <rapid/utilis.h>

#include "huffman.h"
#include "hpack.h"

static std::vector<HpackEntry> const s_staticTable {
	{ ":authority", "" },
	{ ":method", "GET" },
	{ ":method", "POST" },
	{ ":path", "/" },
	{ ":path", "/index.html" },
	{ ":scheme", "http" },
	{ ":scheme", "https" },
	{ ":status", "200" },
	{ ":status", "204" },
	{ ":status", "206" },
	{ ":status", "304" },
	{ ":status", "400" },
	{ ":status", "404" },
	{ ":status", "500" },
	{ "Accept-Charset", "" },
	{ "Accept-Encoding", "gzip, deflate" },
	{ "Accept-Language", "" },
	{ "Accept-Ranges", "" },
	{ "Accept", "" },
	{ "Access-Control-Allow-Origin", "" },
	{ "Age", "" },
	{ "Allow", "" },
	{ "Authorization", "" },
	{ "Cache-Control", "" },
	{ "Content-Disposition", "" },
	{ "Content-Encoding", "" },
	{ "Content-Language", "" },
	{ "Content-Length", "" },
	{ "Content-Location", "" },
	{ "Content-Range", "" },
	{ "Content-Type", "" },
	{ "Cookie", "" },
	{ "Date", "" },
	{ "Etag", "" },
	{ "Expect", "" },
	{ "Expires", "" },
	{ "From", "" },
	{ "Host", "" },
	{ "If-Match", "" },
	{ "If-Modified-Since", "" },
	{ "If-None-Match", "" },
	{ "If-Range", "" },
	{ "If-Unmodified-Since", "" },
	{ "Last-Modified", "" },
	{ "Link", "" },
	{ "Location", "" },
	{ "Max-Forwards", "" },
	{ "Proxy-Authenticate", "" },
	{ "Proxy-Authorization", "" },
	{ "Range", "" },
	{ "Referer", "" },
	{ "Refresh", "" },
	{ "Retry-After", "" },
	{ "Server", "" },
	{ "Set-Cookie", "" },
	{ "Strict-Transport-Security", "" },
	{ "Transfer-Encoding", "" },
	{ "User-Agent", "" },
	{ "Vary", "" },
	{ "Via", "" },
	{ "WWW-Authenticate", "" }
};

static size_t const H2_ENTRY_OVERHEAD = 32;

static uint8_t const s_preFixLimits[] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

class StaticIndexTable {
public:
	enum {
		LOOKUP_RESULT_FIND_NAME_AND_VALUE,
		LOOKUP_RESULT_ONLY_NAME,
		LOOKUP_RESULT_NOT_EXSIT,
	};

	StaticIndexTable() {
		for (uint8_t i = 0; i < s_staticTable.size(); ++i) {
			headerNameTable.insert(std::make_pair(s_staticTable[i].name_, i + 1));
		}
	}

	int lookup(std::string const &name, std::string const &value, uint8_t &index) const throw() {
		auto range = headerNameTable.equal_range(name);

		auto itr = range.first;
		if (itr == range.second) {
			return LOOKUP_RESULT_NOT_EXSIT;
		}

		for (; itr != range.second; ++itr) {
			index = (*itr).second;
			auto const & entry = s_staticTable[index - 1];
			if (rapid::utils::caseInsensitiveCompare(entry.value_.c_str(), value.c_str()) == 0) {
				return LOOKUP_RESULT_FIND_NAME_AND_VALUE;
			}
		}
		return LOOKUP_RESULT_ONLY_NAME;
	}

	HpackEntry const & getEntry(int index) const noexcept {
		return s_staticTable[index - 1];
	}

	size_t size() const noexcept {
		return s_staticTable.size();
	}

private:
	std::multimap<std::string, uint8_t, rapid::utils::CaseInsensitiveCompare> headerNameTable;
};

static StaticIndexTable const s_staticIndexTable;

HpackEntry::HpackEntry()
	: noIndexing_(false) {
}

HpackEntry::HpackEntry(char const *name, char const *value)
	: name_(name)
	, value_(value) {
}

size_t HpackEntry::bytes() const noexcept {
	return H2_ENTRY_OVERHEAD + name_.size() + value_.size();
}

bool HpackEntry::hasValue() const noexcept {
	return !value_.empty();
}

bool HpackEntry::operator==(HpackEntry const& other) const noexcept {
	return name_ == other.name_ && value_ == other.value_;
}

bool HpackEntry::operator<(HpackEntry const& other) const noexcept {
	if (name_ != other.name_) {
		return name_ < other.name_;
	}
	return value_ < other.value_;
}

bool HpackEntry::operator>(HpackEntry const& other) const noexcept {
	if (name_ != other.name_) {
		return name_ > other.name_;
	}
	return value_ > other.value_;
}

static inline bool isWithoutIndexing(HpackEntry const &entry) {
	if (entry.name_ == ":path"
		|| entry.name_ == "content-length"
		|| entry.name_ == "if-modified-since"
		|| entry.name_ == "if-none-match"
		|| entry.name_ == "location"
		|| entry.name_ == "set-cookie") {
		return true;
	}
	return false;
}

static inline bool isNeverIndexing(HpackEntry const &entry) {
	if (entry.name_ == "authorization"
		|| (entry.value_ == "cookie" && entry.value_.length() < 20)
		|| entry.noIndexing_) {
		return true;
	}
	return false;
}

void HpackEntry::clear() {
	name_.clear();
	value_.clear();
}

bool HpackEntry::isValid() const {
	return !name_.empty() || !value_.empty();
}

static inline uint32_t decodeInteger(rapid::IoBuffer* buffer, int prefixBits) {
	auto const prefixMax = s_preFixLimits[prefixBits];
	
	uint32_t const byte = buffer->peek()[0] & prefixMax;

	buffer->retrieve(1);

	if (byte < prefixMax) {
		return byte;
	}
	
	auto integer = byte;
	
	auto M = 0;
	
	for (;;) {
		integer += (buffer->peek()[0] & 0x7F) * (1 << M);
		if ((buffer->peek()[0] & 0x80) != 0x80) {
			break;
		}
		buffer->retrieve(1);
		M += 7;
	}
	
	buffer->retrieve(1);
	return integer;
}

static inline void encodeInteger(rapid::IoBuffer* buffer, int prefixBits, uint32_t integer) {
	auto const const prefixMax = s_preFixLimits[prefixBits];

	if (integer < prefixMax) {
		rapid::writeData(buffer, static_cast<uint8_t>(integer));
	} else {
		rapid::writeData(buffer, prefixMax);
		
		prefixBits -= prefixMax;
	
		while (prefixBits >= 128) {
			rapid::writeData(buffer, static_cast<uint8_t>(prefixBits % 128 + 128));
			prefixBits /= 128;
		}
		rapid::writeData(buffer, static_cast<uint8_t>(prefixBits));
	}
}

static inline std::string readString(rapid::IoBuffer* buffer) {
	std::string str;

	auto const isHuffmanEncoding = (buffer->peek()[0] & 0x80) != 0x00;
	auto const length = decodeInteger(buffer, 7);
	
	if (!length) {
		RAPID_LOG_INFO() << "\r\n" << *buffer;
	}
	
	RAPID_ENSURE(length != 0);
	
	if (isHuffmanEncoding) {
		decodeHuffman(buffer, length, str);
	} else {
		str.assign(buffer->peek(), length);
		buffer->retrieve(length);
	}
	return str;
}

static inline void writePairString(rapid::IoBuffer* buffer, std::string const &name, std::string const &value) {
	if (!name.empty()) {
		encodeInteger(buffer, 7, static_cast<uint32_t>(name.length()));
		rapid::writeData<rapid::utils::NoEndian>(buffer, reinterpret_cast<uint8_t const*>(name.c_str()), name.length());
	}
	if (!value.empty()) {
		encodeInteger(buffer, 7, static_cast<uint32_t>(value.length()));
		rapid::writeData<rapid::utils::NoEndian>(buffer, reinterpret_cast<uint8_t const*>(value.c_str()), value.length());
	}
}

IndexTable::IndexTable()
	: currentSize_(0)
	, headerTableSize_(4096) {
}

HpackEntry IndexTable::lookup(int index) const {
	if (index < s_staticIndexTable.size()) {
		return s_staticIndexTable.getEntry(index);
	}
	index -= s_staticIndexTable.size();
	return dynamicTable_[index - 1];
}

void IndexTable::addHpackEntry(HpackEntry &entry) {
	dynamicTable_.emplace_front(entry);
	currentSize_ += entry.bytes();
	evict();
}

void IndexTable::evict() {
	while (currentSize_ > headerTableSize_) {
		auto const &entry = dynamicTable_.back();
		currentSize_ -= entry.bytes();
		dynamicTable_.pop_back();
	}
}

void IndexTable::setHeaderTableSize(uint32_t tableSize) {
	headerTableSize_ = tableSize;
	evict();
}

uint32_t IndexTable::getHeaderTableSize() const {
	return headerTableSize_;
}

Http2Hpack::Http2Hpack() {
}

/*

   0   1   2   3   4   5   6   7
 +---+---+---+---+---+---+---+---+
 | 0 | 0 | 1 | 1 |       0       |
 +---+---------------------------+
      Reference Set Emptying

   0   1   2   3   4   5   6   7
 +---+---+---+---+---+---+---+---+
 | 0 | 0 | 1 | 0 | Max size (7+) |
 +---+---------------------------+
  Maximum Header Table Size Change

*/
void Http2Hpack::parseHeaderUpdate(rapid::IoBuffer* buffer, uint8_t firstByte) {
	if (firstByte == 0x30) {
		// Requested reference set emptying
		buffer->retrieve(1);
		return;
	}

	// Maximum header table size change:
	auto headerTableSize = decodeInteger(buffer, 4);
	indexTable_.setHeaderTableSize(headerTableSize);
	RAPID_LOG_INFO() << "Maximum header table size change: " << headerTableSize;
}

/*

   0   1   2   3   4   5   6   7
 +---+---+---+---+---+---+---+---+
 | 1 |        Index (7+)         |
 +---+---------------------------+
      Indexed Header Field

*/
void Http2Hpack::parseIndexedHeaderFiled(rapid::IoBuffer* buffer, HpackEntry &entry) {
	// 預設的staic table/dynamic table已有預先定義的name和value
	auto index = decodeInteger(buffer, 7);
	entry = indexTable_.lookup(index);
}

/*

    <----------  Index Address Space ---------->
    <-- Static  Table -->  <-- Dynamic Table -->
    +---+-----------+---+  +---+-----------+---+
    | 1 |    ...    | s |  |s+1|    ...    |s+k|
    +---+-----------+---+  +---+-----------+---+
                           ^                   |
                           |                   V
                     Insertion Point      Dropping Point



  LITERAL HEADER FIELD WITH INCREMENTAL INDEXING

    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 1 |      Index (6+)       |
  +---+---+---+-------------------+
  | H |     Value Length (7+)     |
  +-------------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
             Indexed Name


    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 1 |           0           |
  +---+---+---+-------------------+
  | H |     Value Length (7+)     |
  +-------------------------------+
  |  Name String (Length octets)  |
  +-------------------------------+
  | H |     Value Length (7+)     |
  +-------------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
              New Name


  LITERAL HEADER FIELD WITHOUT INDEXING

    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 0 | 0 | 0 |  Index (4+)   |
  +---+---+-----------------------+
  | H |     Value Length (7+)     |
  +---+---------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
             Indexed Name


    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 0 | 0 | 0 |       0       |
  +---+---+-----------------------+
  | H |     Name Length (7+)      |
  +---+---------------------------+
  |  Name String (Length octets)  |
  +---+---------------------------+
  | H |     Value Length (7+)     |
  +---+---------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
              New Name



  LITERAL HEADER FIELD NEVER INDEXED

    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 0 | 0 | 1 |  Index (4+)   |
  +---+---+-----------------------+
  | H |     Value Length (7+)     |
  +---+---------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
             Indexed Name

    0   1   2   3   4   5   6   7
  +---+---+---+---+---+---+---+---+
  | 0 | 0 | 0 | 1 |       0       |
  +---+---+-----------------------+
  | H |     Name Length (7+)      |
  +---+---------------------------+
  |  Name String (Length octets)  |
  +---+---------------------------+
  | H |     Value Length (7+)     |
  +---+---------------------------+
  | Value String (Length octets)  |
  +-------------------------------+
              New Name

*/
std::map<std::string, std::string> Http2Hpack::decodeHeader(rapid::IoBuffer* buffer, uint32_t bufferLen) {
	std::map<std::string, std::string> referenceSet;
	
	int totalBytesRead = buffer->readable() - bufferLen;

	for (size_t i = 0; !buffer->isEmpty() && buffer->readable() > totalBytesRead;) {
		uint8_t const firstByte = buffer->peek()[0];

		auto const isWithoutIndexing = (firstByte & 0xf0) == 0;
		auto const isNeverIndexing = (firstByte & 0xf0) == 0x10;
		auto const isIndexedField = (firstByte & 0x80) != 0;
		auto const isIncrementalIndexing = (firstByte & 0xc0) == 0x40;
		auto const isDynamicTableSizeUpdate = (firstByte & 0xe0) == 0x20;
		
		HpackEntry entry;
		entry.noIndexing_ = isNeverIndexing;
		
		uint8_t index = 0;
		
		if (isIndexedField) {
			parseIndexedHeaderFiled(buffer, entry);
			RAPID_LOG_TRACE() << std::left << std::setw(22) << "Indexed Field" << std::setw(30) << entry.name_;
		} else if (isIncrementalIndexing) {
			index = decodeInteger(buffer, 6);
			parseHeaderPair(buffer, index, entry);
			indexTable_.addHpackEntry(entry);
			RAPID_LOG_TRACE() << std::left << std::setw(22) << "Incremental Indexing" << std::setw(30) << entry.name_;
		} else if (isWithoutIndexing) {
			index = decodeInteger(buffer, 4);
			parseHeaderPair(buffer, index, entry);
			RAPID_LOG_TRACE() << std::left << std::setw(22) << "Without Indexing" << std::setw(30) << entry.name_;
		} else if (isNeverIndexing) {
			index = decodeInteger(buffer, 4);
			parseHeaderPair(buffer, index, entry);
			RAPID_LOG_TRACE() << std::left << std::setw(22) << "Never Indexing" << std::setw(30) << entry.name_;
		} else if (isDynamicTableSizeUpdate) {
			parseHeaderUpdate(buffer, firstByte);
		}

		referenceSet[entry.name_] = entry.value_;
	}
	return referenceSet;
}

void Http2Hpack::parseHeaderPair(rapid::IoBuffer* buffer, uint8_t index, HpackEntry &entry) {
	if (index > 0) {
		// 預設header在static table有設定name
		entry.name_ = indexTable_.lookup(index).name_;
	} else {
		// 預設header在static table沒有name需要設定新的name和value
		entry.name_ = readString(buffer);
	}
	entry.value_ = readString(buffer);
}

void Http2Hpack::encodeHeader(rapid::IoBuffer* buffer, std::string const &name, std::string const &value) {
	uint8_t index = 0;

	auto ret = s_staticIndexTable.lookup(name, value, index);
	if (ret == StaticIndexTable::LOOKUP_RESULT_NOT_EXSIT) {
		// Literal Header Field without Indexing — New Name
		rapid::writeData(buffer, static_cast<uint8_t>(0x00));
		writePairString(buffer, name, value);
		RAPID_LOG_TRACE() << "Indexing — New Name: " << name;
		return;
	}
	
	auto &entry = s_staticIndexTable.getEntry(index);
	if (isNeverIndexing(entry)) {
		// Literal Header Field Never Indexed — Indexed Name
		rapid::writeData(buffer, static_cast<uint8_t>(0x10));
		writePairString(buffer, entry.name_, value);
		RAPID_LOG_TRACE() << "Never Indexed — Indexed Name: " << entry.name_;
		return;
	}

	if (isWithoutIndexing(entry)) {
		// Literal Header Field without Indexing — New Name
		rapid::writeData(buffer, static_cast<uint8_t>(0x00));
		writePairString(buffer, entry.name_, value);
		RAPID_LOG_TRACE() << "Without Indexed — Indexed Name: " << entry.name_;
		return;
	}

	auto pData = buffer->writeData();
	if (ret == StaticIndexTable::LOOKUP_RESULT_FIND_NAME_AND_VALUE) {
		// Indexed Header Field Representation			
		encodeInteger(buffer, 7, index);
		*pData |= 0x80;
		RAPID_LOG_TRACE() << "Representation: " << name;
	} else {
		// Literal Header Field with Incremental Indexing — Indexed Name
		encodeInteger(buffer, 6, index);
		*pData |= 0x40;
		writePairString(buffer, "", value);
		RAPID_LOG_TRACE() << "Incremental Indexing — Indexed Name: " << rapid::utils::toLower(entry.name_);
	}
}


