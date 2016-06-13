//---------------------------------------------------------------------------------------------------------------------
// Copyright 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <locale>
#include <codecvt>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

namespace rapid {

namespace utils {

// See : http://en.cppreference.com/w/cpp/locale/wstring_convert/~wstring_convert
// utility wrapper to adapt locale-bound facets for wstring/wbuffer convert.
template <typename Facet>
struct DeleteableFacet : public Facet {
	template <class ...Args>
	explicit DeleteableFacet(Args&& ...args)
		: Facet(std::forward<Args>(args)...) {
	}
};

extern int caseInsensitiveCompare(char const * __restrict s1, char const * __restrict s2);

inline bool caseInsensitiveCompare(std::string const & s1, std::string const & s2) {
	return caseInsensitiveCompare(s1.c_str(), s2.c_str()) == 0;
}

struct CaseInsensitiveCompare : public std::binary_function<std::string, std::string, bool> {
	inline bool operator()(std::string const & s1, std::string const & s2) const {
		return caseInsensitiveCompare(s1.c_str(), s2.c_str()) < 0;
	}
};

class HorspoolSearch : public std::binary_function<char const *, size_t, bool> {
public:
	explicit HorspoolSearch(std::string const &s);

	inline size_t patternLength() const throw() {
		return pattern.length();
	}

	size_t operator()(char const *str, size_t strLen) const;
private:
	std::string pattern;
	std::vector<size_t> occTable;
};

inline size_t indexOf(std::string const &str, std::string const &subStr) {
	HorspoolSearch search(subStr);
	return search(str.c_str(), str.length());
}

inline std::wstring fromBytes(std::string const &bytes, std::string const &facetName) {
	using Codecvt = DeleteableFacet<std::codecvt<wchar_t, char, std::mbstate_t>>;
	std::wstring_convert<Codecvt> convert(new Codecvt(facetName.c_str()));
	return convert.from_bytes(bytes);
}

inline std::wstring fromBytes(std::string const &bytes) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
	return convert.from_bytes(bytes);
}

inline std::string formUtf16(std::wstring const &utf16) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
	return convert.to_bytes(utf16);
}

inline std::string formUtf16(std::wstring const &utf16, std::string const &facetName) {
	using Codecvt = DeleteableFacet<std::codecvt<wchar_t, char, std::mbstate_t>>;
	std::wstring_convert<Codecvt> convert(new Codecvt(facetName.c_str()));
	return convert.to_bytes(utf16);
}

template <typename CharT>
inline void replace(std::basic_string<CharT> &s,
	std::basic_string<CharT> const &dim,
	std::basic_string<CharT> const &r,
	bool overlapp = true) {
	for (auto i = s.find(dim); i != std::basic_string<CharT>::npos; i = s.find(dim, i + 1)) {
		if (overlapp) {
			s.replace(i, r.length(), r);
		} else {
			std::basic_string<CharT> subStr = s.substr(i + dim.length());
			s.erase(i);
			s.append(r);
			s.append(subStr);
		}
	}
}

template <typename CharT, std::size_t N1>
inline void replace(std::basic_string<CharT> &s,
	CharT const (&dim)[N1],
	std::basic_string<CharT> const &r,
	bool overlapp = true) {
	replace(s, std::basic_string<CharT>(dim), r, overlapp);
}

template <typename CharT, std::size_t N1, std::size_t N2>
inline void replace(std::basic_string<CharT> &s,
	CharT const (&dim)[N1],
	CharT const (&r)[N2],
	bool overlapp = true) {
	replace(s, std::basic_string<CharT>(dim), std::basic_string<CharT>(r), overlapp);
}

template <typename CharT>
inline void toLower(std::basic_string<CharT> &str) {
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

template <typename CharT>
inline std::basic_string<CharT> toLower(std::basic_string<CharT> const &str) {
	auto temp = str;
	std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
	return temp;
}

template <typename CharT>
inline void trimLeft(std::basic_string<CharT> &str, CharT ch = ' ') {
	str.erase(0, str.find_first_not_of(ch));
}

template <typename CharT>
inline void trimRight(std::basic_string<CharT> &str, CharT ch = ' ') {
	str.erase(str.find_last_not_of(ch) + 1);
}

template <typename CharT>
inline std::basic_string<CharT>& trim(std::basic_string<CharT> &str) {
	trimLeft(str);
	trimRight(str);
	return str;
}

template <typename CharT>
inline std::basic_string<CharT> trim(std::basic_string<CharT> const &input, CharT token = ' ') {
	typename std::basic_string<CharT>::size_type first = input.find_first_not_of(token);
	typename std::basic_string<CharT>::size_type last = input.find_last_not_of(token);

	if (first == std::basic_string<CharT>::npos && last == std::basic_string<CharT>::npos) {
		return std::basic_string<CharT>();
	}
	return input.substr(first, last - first + 1);
}

template <typename CharT, typename Traits, typename Container>
inline Container& split(std::basic_string<CharT, Traits> const &s, Container& result, HorspoolSearch const &search) {
	typedef typename Container::value_type value_type;
	typedef typename std::basic_string<CharT, Traits>::const_iterator const_iterator;
	if (s.empty()) {
		return result;
	}

	size_t nextStart = 0;
	size_t strLen = s.length();
	size_t npos = 0;
	
	for (;;) {
		npos = strLen - nextStart;
		auto findPos = search(s.c_str() + nextStart, npos);
		if (!findPos || findPos == npos) {
			break;
		}
		result.push_back(s.substr(nextStart, findPos));
		nextStart += findPos + search.patternLength();
	}

	return result;
}

template <typename CharT, typename Traits, typename Container, typename Predicate>
inline Container& split(std::basic_string<CharT, Traits> const &s,
	Container& result,
	Predicate f,
	bool removeEmptyEntries = false) {
	typedef typename Container::value_type value_type;
	typedef typename std::basic_string<CharT, Traits>::const_iterator const_iterator;
	
	if (s.empty()) {
		return result;
	}
	
	std::basic_string<CharT, Traits> tmp;
	std::insert_iterator<Container> it(result, result.end());
	
	for (const_iterator pos = s.begin(); pos != s.end(); ++pos) {
		if (!f(*pos)) {
			tmp += *pos;
		} else if (!removeEmptyEntries || !tmp.empty()) {
			*it = tmp;
			++it;
			tmp.erase();
		}
	}
	
	if (!tmp.empty()) {
		*it = tmp;
		++it;
	}
	return result;
}

std::string randomString(size_t strLen);

std::string byteFormat(uint64_t bytes, int precision = 2);

std::string bin2hex(std::vector<uint8_t> const &bin);

std::string urlEncode(std::wstring const &wideString);

std::wstring urlDecode(std::string const &multiBytesString);


}

}
