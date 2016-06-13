//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

#include <rapid/utils/horspool.h>
#include <rapid/utils/stringutilis.h>

namespace rapid {

namespace utils {

static uint8_t const s_charmap[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xe1, 0xe2, 0xe3, 0xe4, 0xc5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

static char const s_alphanums[] = {
	"0123456789"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
};

static inline char toHex(uint8_t ch) {
	if (ch < 10) {
		return ch + '0';
	}
	return ch - 10 + 'A';
}

static inline uint8_t fromHex(uint8_t ch) {
	if (ch <= '9' && ch >= '0')
		ch -= '0';
	else if (ch <= 'f' && ch >= 'a')
		ch -= 'a' - 10;
	else if (ch <= 'F' && ch >= 'A')
		ch -= 'A' - 10;
	else
		ch = 0;
	return ch;
}

int caseInsensitiveCompare(char const * __restrict s1, char const * __restrict s2) {
	uint8_t u1 = 0;
	uint8_t u2 = 0;

	for (;; s1++, s2++) {
		u1 = static_cast<uint8_t>(*s1);
		u2 = static_cast<uint8_t>(*s2);
		if ((u1 == '\0') || (s_charmap[u1] != s_charmap[u2])) {
			break;
		}
	}
	return (s_charmap[u1] - s_charmap[u2]);
}

HorspoolSearch::HorspoolSearch(std::string const &s) {
	pattern = s;
	occTable = CreateOccTable(reinterpret_cast<unsigned char const *>(pattern.c_str()), pattern.length());
}

size_t HorspoolSearch::operator()(char const * str, size_t strLen) const {
	return SearchInHorspool(reinterpret_cast<unsigned char const * __restrict>(str),
		strLen,
		occTable,
		reinterpret_cast<unsigned char const * __restrict>(pattern.c_str()),
		pattern.length());
}

std::string randomString(size_t strLen) {
	static std::mt19937 rg(std::chrono::system_clock::now().time_since_epoch().count());
	static std::uniform_int_distribution<> generator(0, _countof(s_alphanums) - 1);

	std::string s;
	s.reserve(strLen);
	
	while (strLen--) {
		s += s_alphanums[generator(rg)];
	}
	return s;
}

std::string byteFormat(uint64_t bytes, int precision) {
	static char const * const formatters[] = {
		" B",
		" KB",
		" MB",
		" GB",
		" TB"
	};
	
	auto doubleBytes = static_cast<double>(bytes);
	auto numConversions = 0;
	while (doubleBytes > 1000) {
		doubleBytes /= 1024;
		numConversions++;
	}

	std::string units;
	if ((0 <= numConversions) && (numConversions <= _countof(formatters))) {
		units = formatters[numConversions];
	}
	
	std::ostringstream ostr;
	ostr.setf(std::ios::fixed);
	ostr.precision(precision);
	ostr << doubleBytes << units;
	return ostr.str();
}

std::string bin2hex(std::vector<uint8_t> const &bin) {
	std::string str;
	
	str.reserve(bin.size() * 2);

	for (auto c : bin) {
		str += toHex(c >> 4);
		str += toHex(c & 0x0F);
	}
	return str;
}

std::string urlEncode(std::wstring const & wideString) {
	auto utf8String = formUtf16(wideString);

	std::ostringstream result;
	std::string temp;

	for (size_t i = 0; i < utf8String.size(); ) {
		if (static_cast<unsigned char>(utf8String[i]) <= static_cast<unsigned char>(0x7f)) {
			if ((utf8String[i] >= '0' && utf8String[i] <= '9') ||
				(utf8String[i] >= 'A' && utf8String[i] <= 'Z') ||
				(utf8String[i] >= 'a' && utf8String[i] <= 'z')) {
				temp += utf8String[i];
				++i;
			} else if (utf8String[i] == ' ') {
				temp += '+';
			} else {
				result.str("");
				result << "%"
					<< std::setfill('0')
					<< std::setw(2)
					<< std::hex
					<< static_cast<uint32_t>(static_cast<uint8_t>(utf8String[i]));
				temp += result.str();
			}
			++i;
		} else {
			result.str("");
			for (size_t j = 0; j < 3 && i < utf8String.size(); ++j, ++i) {
				result << "%"
					<< std::setfill('0')
					<< std::setw(2)
					<< std::hex
					<< static_cast<uint32_t>(static_cast<uint8_t>(utf8String[i]));
			}
			temp += result.str();
		}
	}

	return temp;
}

std::wstring urlDecode(std::string const &multiBytesString) {
	std::string temp;

	for (size_t i = 0; i < multiBytesString.size(); ++i) {
		if (multiBytesString[i] == '+') {
			temp += ' ';
		} else if (multiBytesString[i] == '%' && multiBytesString.size() > i + 2) {
			auto ch1 = fromHex(multiBytesString[i + 1]);
			auto ch2 = fromHex(multiBytesString[i + 2]);
			temp += ((ch1 << 4) | ch2);
			i += 2;
		} else {
			temp += multiBytesString[i];
		}
	}
	return fromBytes(temp);
}

}

}
