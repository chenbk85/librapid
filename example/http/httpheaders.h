//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

enum HeaderCode {
	HTTP_HEADER_ACCEPT = 0,
	HTTP_HEADER_ACCEPT_CHARSET,
	HTTP_HEADER_ACCEPT_DATETIME,
	HTTP_HEADER_ACCEPT_ENCODING,
	HTTP_HEADER_ACCEPT_LANGUAGE,
	HTTP_HEADER_ACCEPT_RANGES,
	HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS,
	HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS,
	HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS,
	HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
	HTTP_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS,
	HTTP_HEADER_ACCESS_CONTROL_MAX_AGE,
	HTTP_HEADER_ACCESS_CONTROL_REQUEST_HEADERS,
	HTTP_HEADER_ACCESS_CONTROL_REQUEST_METHOD,
	HTTP_HEADER_AGE,
	HTTP_HEADER_ALLOW,
	HTTP_HEADER_AUTHORIZATION,
	HTTP_HEADER_CACHE_CONTROL,
	HTTP_HEADER_CONNECTION,
	HTTP_HEADER_CONTENT_DISPOSITION,
	HTTP_HEADER_CONTENT_ENCODING,
	HTTP_HEADER_CONTENT_LANGUAGE,
	HTTP_HEADER_CONTENT_LENGTH,
	HTTP_HEADER_CONTENT_LOCATION,
	HTTP_HEADER_CONTENT_MD5,
	HTTP_HEADER_CONTENT_RANGE,
	HTTP_HEADER_CONTENT_TYPE,
	HTTP_HEADER_COOKIE,
	HTTP_HEADER_DNT,
	HTTP_HEADER_DATE,
	HTTP_HEADER_ETAG,
	HTTP_HEADER_EXPECT,
	HTTP_HEADER_EXPIRES,
	HTTP_HEADER_FROM,
	HTTP_HEADER_FRONT_END_HTTPS,
	HTTP_HEADER_HOST,
	HTTP_HEADER_IF_MATCH,
	HTTP_HEADER_IF_MODIFIED_SINCE,
	HTTP_HEADER_IF_NONE_MATCH,
	HTTP_HEADER_IF_RANGE,
	HTTP_HEADER_IF_UNMODIFIED_SINCE,
	HTTP_HEADER_KEEP_ALIVE,
	HTTP_HEADER_LAST_MODIFIED,
	HTTP_HEADER_LINK,
	HTTP_HEADER_LOCATION,
	HTTP_HEADER_MAX_FORWARDS,
	HTTP_HEADER_ORIGIN,
	HTTP_HEADER_P3P,
	HTTP_HEADER_PRAGMA,
	HTTP_HEADER_PROXY_AUTHENTICATE,
	HTTP_HEADER_PROXY_AUTHORIZATION,
	HTTP_HEADER_PROXY_CONNECTION,
	HTTP_HEADER_RANGE,
	HTTP_HEADER_REFERER,
	HTTP_HEADER_REFRESH,
	HTTP_HEADER_RETRY_AFTER,
	HTTP_HEADER_SERVER,
	HTTP_HEADER_SET_COOKIE,
	HTTP_HEADER_STRICT_TRANSPORT_SECURITY,
	HTTP_HEADER_TE,
	HTTP_HEADER_TIMESTAMP,
	HTTP_HEADER_TRAILER,
	HTTP_HEADER_TRANSFER_ENCODING,
	HTTP_HEADER_UPGRADE,
	HTTP_HEADER_USER_AGENT,
	HTTP_HEADER_VIP,
	HTTP_HEADER_VARY,
	HTTP_HEADER_VIA,
	HTTP_HEADER_WWW_AUTHENTICATE,
	HTTP_HEADER_WARNING,
	HTTP_HEADER_X_ACCEL_REDIRECT,
	HTTP_HEADER_X_CONTENT_SECURITY_POLICY_REPORT_ONLY,
	HTTP_HEADER_X_CONTENT_TYPE_OPTIONS,
	HTTP_HEADER_X_FORWARDED_FOR,
	HTTP_HEADER_X_FORWARDED_PROTO,
	HTTP_HEADER_X_FRAME_OPTIONS,
	HTTP_HEADER_X_POWERED_BY,
	HTTP_HEADER_X_REAL_IP,
	HTTP_HEADER_X_REQUESTED_WITH,
	HTTP_HEADER_X_UA_COMPATIBLE,
	HTTP_HEADER_X_WAP_PROFILE,
	HTTP_HEADER_X_XSS_PROTECTION,
	HTTP_HEADER_UPGRADE_INSECURE_REQUESTS,
	HTTP_HEADER_NONE,
};

class HttpHeaderName {
public:
	explicit HttpHeaderName(std::string &&name);

	explicit HttpHeaderName(char const *name);
	
	__forceinline uint32_t hash() const noexcept {
		return hash_;
	}

	__forceinline std::string const & str() const noexcept {
		return name_;
	}

private:
	std::string name_;
	uint32_t hash_;
};

class HttpHeaders {
public:
	HttpHeaders();
	
	~HttpHeaders();

	std::string const& get(HttpHeaderName const &header) const;

	void add(HttpHeaderName const &header, std::string const &value);

	void add(HttpHeaderName const &name, int64_t value);

	void add(std::string const &name, std::string const &value) {
		add(name, value.c_str(), static_cast<uint32_t>(value.length()));
	}

	void add(std::string const &name, char const *value, uint32_t valueLength);
	
	bool has(HttpHeaderName const &header) const noexcept;

	void remove(HttpHeaderName const &header);

	void clear();

	template <typename Lambda>
	void foreach(Lambda &&lambda) const {
		auto numHeaderNames = static_cast<uint32_t>(headerValues_.size());
		for (uint32_t i = 0; i < numHeaderNames; ++i) {
			lambda(*headerNames_[i].second, headerValues_[i]);
		}
	}

private:
	std::vector<std::pair<uint32_t, std::string const *>> headerNames_;
	std::vector<std::string> headerValues_;
};

__forceinline bool HttpHeaders::has(HttpHeaderName const& header) const noexcept {
	for (auto const &code : headerNames_) {
		if (code.first == header.hash())
			return true;
	}
	return false;
}
