//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <rapid/objectpool.h>
#include <rapid/utils/singleton.h>

#include "http2/http2codec.h"

#include "filecachemanager.h"

#include "predeclare.h"
#include "httpscontext.h"
#include "httpstaticheadertable.h"
#include "httpmessage.h"
#include "httpheaders.h"

static uint32_t constexpr SIZE_4KB = 4 * 1024;
static uint32_t constexpr SIZE_16KB = 16 * 1024;
static uint32_t constexpr SIZE_64KB = 64 * 1024;
static uint32_t constexpr SIZE_1MB = 1024 * 1024;
static uint32_t constexpr SIZE_10MB = 10 * SIZE_1MB;

using HttpRequestPool = rapid::ObjectPool<HttpRequest>;
using HttpResponsePool = rapid::ObjectPool<HttpResponse>;
using Http2RequestPool = rapid::ObjectPool<Http2Request>;
using Http2ResponsePool = rapid::ObjectPool<Http2Response>;
using HttpContextPool = rapid::ObjectPool<HttpContext>;
using HttpsContextPool = rapid::ObjectPool<HttpsContext>;
using Http2StreamPool = rapid::ObjectPool<Http2Stream>;

class HttpServerConfigFacade : public rapid::utils::Singleton<HttpServerConfigFacade> {
public:
	HttpServerConfigFacade();

	HttpServerConfigFacade(const HttpServerConfigFacade &) = delete;
	HttpServerConfigFacade& operator=(HttpServerConfigFacade const &) = delete;

	void loadIniConfigFile(std::string const &filePath);

	void loadXmlConfigFile(std::string const &filePath);

	FileCacheManager& getFileCacheManager() noexcept;

	HttpStaticHeaderTable const & getHeadersTable() const;

	HttpRequestPool& getHttpRequestPool() noexcept;

	HttpResponsePool& getHttpResponsePool() noexcept;

	Http2RequestPool& getHttp2RequestPool() noexcept;

	Http2ResponsePool& getHttp2ResponsePool() noexcept;

	Http2StreamPool& getHttp2StreamPool() noexcept;

	HttpContextPool& getHttpContextPool() noexcept;

	HttpsContextPool& getHttpsContextPool() noexcept;

	std::string getIndexFileName() const;

	std::string getTempFilePath() const;

	std::string getServerName() const;

	std::string getRootPath() const;

	std::string getHost() const;

	std::string getPrivateKeyFilePath() const;

	std::string getCertificateFilePath() const;

	bool isUseHttp2() const noexcept;

	bool isUseSSL() const noexcept;

	int getListenPort() const noexcept;

	uint32_t getBufferSize() const noexcept;

	uint32_t getMaxUserConnection() const noexcept;

	uint32_t getInitalUserConnection() const noexcept;

	uint16_t getNumaNode() const noexcept;

private:
	FileCacheManager fileCacheManager_;
	bool enableHttp2Proto_ : 1;
	bool enableSSLProto_ : 1;
	int listenPort_;
	bool upgradeableHttp2_ : 1;
	uint16_t numaNode_;
	uint32_t bufferSize_;
	uint32_t maxUserConnection_;
	uint32_t initialUserConnection_;
	std::string privateKeyFilePath_;
	std::string certificateFilePath_;
	std::string tempFilePath_;
	std::string serverName_;
	std::string rootPath_;
	std::string host_;
	std::string indexFileName_;
	HttpStaticHeaderTable headersTable_;
	HttpRequestPool httpRequestPool_;
	HttpResponsePool httpResponsePool_;
	Http2RequestPool http2RequestPool_;
	Http2ResponsePool http2ResponsePool_;
	Http2StreamPool http2StreamPool_;
	HttpContextPool httpContextPool_;
	HttpsContextPool httpsContextPool_;
};

__forceinline uint32_t HttpServerConfigFacade::getInitalUserConnection() const noexcept {
	return initialUserConnection_;
}

__forceinline uint32_t HttpServerConfigFacade::getMaxUserConnection() const noexcept {
	return maxUserConnection_;
}

__forceinline uint32_t HttpServerConfigFacade::getBufferSize() const noexcept {
	return bufferSize_;
}

__forceinline FileCacheManager& HttpServerConfigFacade::getFileCacheManager() noexcept {
	return fileCacheManager_;
}

__forceinline HttpRequestPool& HttpServerConfigFacade::getHttpRequestPool() noexcept {
	return httpRequestPool_;
}

__forceinline HttpResponsePool& HttpServerConfigFacade::getHttpResponsePool() noexcept {
	return httpResponsePool_;
}

__forceinline Http2RequestPool& HttpServerConfigFacade::getHttp2RequestPool() noexcept {
	return http2RequestPool_;
}

__forceinline Http2ResponsePool& HttpServerConfigFacade::getHttp2ResponsePool() noexcept {
	return http2ResponsePool_;
}

__forceinline HttpContextPool& HttpServerConfigFacade::getHttpContextPool() noexcept {
	return httpContextPool_;
}

__forceinline Http2StreamPool& HttpServerConfigFacade::getHttp2StreamPool() noexcept {
	return http2StreamPool_;
}

__forceinline HttpsContextPool& HttpServerConfigFacade::getHttpsContextPool() noexcept {
	return httpsContextPool_;
}

__forceinline bool HttpServerConfigFacade::isUseHttp2() const noexcept {
	return enableHttp2Proto_;
}

__forceinline bool HttpServerConfigFacade::isUseSSL() const noexcept {
	return enableSSLProto_;
}

__forceinline int HttpServerConfigFacade::getListenPort() const noexcept {
	return listenPort_;
}

__forceinline uint16_t HttpServerConfigFacade::getNumaNode() const noexcept {
	return numaNode_;
}