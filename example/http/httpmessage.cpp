//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <filesystem>
#include <sstream>
#include <unordered_map>

#include <rapid/utils/singleton.h>
#include <rapid/utilis.h>
#include <rapid/utils/stringutilis.h>
#include <rapid/logging/logging.h>

#include "mime.h"
#include "httpstatuscode.h"
#include "httpserverconfigfacade.h"
#include "httpconstants.h"
#include "filecachemanager.h"
#include "httpmessage.h"

static inline HttpStatus fromHttpStatusCode(HttpStatusCode code) {
	// HTTP status code map
	static std::unordered_map<HttpStatusCode, HttpStatus> const s_statucCodeStringMap{
		{ HTTP_SWITCHING_PROTOCOLS,{ "101", "Switching Protocols" } },
		{ HTTP_OK,{ "200", "OK" } },
		{ HTTP_NOT_FOUND,{ "204", "Not found" } },
		{ HTTP_PARTICAL_CONTENT,{ "206", "Partial Content" } },
		{ HTTP_BAD_REQUEST,{ "400", "Bad Request" } },
	};

	auto itr = s_statucCodeStringMap.find(code);
	RAPID_ENSURE(itr != s_statucCodeStringMap.end());
	return (*itr).second;
}

static inline std::string getFileExt(std::string const &filePath) {
	auto pos = filePath.rfind('.');
	return filePath.substr(pos == std::string::npos ? filePath.length() : pos);
}

void HttpMessage::doSerialize(rapid::IoBuffer* pBuffer) {
	headers_.foreach([pBuffer](std::string const &name, std::string const &value) {
		pBuffer->append(name)
			.append(':')
			.append(value)
			.append(HTTP_CRLF);
	});
	pBuffer->append(HTTP_CRLF);
}

void HttpMessage::doUnserialize(rapid::IoBuffer* pBuffer) {

}

HttpRequest::HttpRequest()
	: keepAlive_(false)
	, totalReadPostSize_(0) {
}

HttpRequest::~HttpRequest() {

}

void HttpRequest::setUri(std::string const &str) {
    uri_.fromString(str);
}

void HttpRequest::setUri(char const *str, size_t length) {
	uri_.fromString(str, length);
}

std::string HttpRequest::method() const {
    return method_;
}

void HttpRequest::setMethod(std::string const &str) {
	method_ = str;
}

uint64_t HttpRequest::getContentLength() const {
    auto const value = get(HTTP_CONETNT_LENGTH);
    return std::strtoull(value.c_str(), nullptr, 10);
}

void HttpRequest::createTempFile() {
	// Randon temp file name.
	auto const tempFilePath = HttpServerConfigFacade::getInstance().getTempFilePath() + "/";
    for (;;) {
		postFile_.open(tempFilePath + rapid::utils::randomString(4) + ".tmp", std::ios::out | std::ios::binary);
		if (postFile_.is_open()) {
            break;
        }
		postFile_.close();
    }

	// Initial multipart reader.
	if (has(HTTP_CONETNT_TYPE)) {
		auto contentType = get(HTTP_CONETNT_TYPE);

		if (contentType.find(HTTP_MULTIPART_FORM_DATA) != std::string::npos) {
		} else if (contentType.find(HTTP_APP_X_WWW_FORM_URLENCODED) != std::string::npos) {
			// TODO: Implement this method.
			throw NotImplementedException();
		} else {
			throw MalformedDataException();
		}

		auto pos = contentType.find(HTTP_BOUNDARY_EQ);
		if (pos == std::string::npos) {
			throw MalformedDataException();
		}

		auto boundary = contentType.substr(pos + HTTP_BOUNDARY_EQ.length());
		multipartReader_.reset();
		multipartReader_.setBoundary(boundary);
		multipartReader_.onPartData = partDataCallback;
		multipartReader_.userData = reinterpret_cast<void*>(this);
	}
	totalReadPostSize_ = 0;
}

void HttpRequest::parseMultipartFormData(char const *buf, size_t bufSize) {
	multipartReader_.feed(buf, bufSize);
}

void HttpRequest::closePostFile() {
    postFile_.close();
}

bool HttpRequest::writeTempFile(rapid::ConnectionPtr pConn) {
	auto *pBuffer = pConn->getReceiveBuffer();
	auto totalContentLength = getContentLength();

	// Content-Length equal zero(0) must return HTTP_BAD_REQUEST!
	if (!totalContentLength) {
		throw HttpRequestErrorException(HTTP_BAD_REQUEST);
	}
	
	for (; !pBuffer->isEmpty() && totalReadPostSize_ != totalContentLength;) {
		auto readableBytes = pBuffer->readable();
		parseMultipartFormData(pBuffer->peek(), readableBytes);
		pBuffer->retrieve(readableBytes);
		totalReadPostSize_ += readableBytes;
	}

	if (totalReadPostSize_ == totalContentLength) {
		closePostFile();
		return true;
	}
	return false;
}

void HttpRequest::setKeepAlive() {
    keepAlive_ = true;
}

bool HttpRequest::isKeepAlive() const {
	// HTTP 1.1 or 2 default is keep-alive
	if (!has(HTTP_CONNECTION)) {
		return true;
	}
	return rapid::utils::caseInsensitiveCompare(get(HTTP_CONNECTION), "keep-alive");
}

bool HttpRequest::isRangeRequest() const {
	return has(HTTP_RANGE);
}

void HttpRequest::getByteRange(int64_t &start, int64_t &end) const {
	auto range = get(HTTP_RANGE);

	range = range.substr(std::string("bytes=").length());
	auto pos = range.find("-");
	auto rangeStart = range.substr(0, pos);
	
	std::string rangeEnd;
	if (pos != std::string::npos) {
		rangeEnd = range.substr(pos + 1);
	}

	rapid::utils::trim(rangeStart);
	rapid::utils::trim(rangeEnd);
	if (!rangeStart.empty()) {
		start = std::strtoll(rangeStart.c_str(), nullptr, 10);
	}

	if (!rangeEnd.empty()) {
		end = std::strtoll(rangeEnd.c_str(), nullptr, 10);
	}
}

Uri const * HttpRequest::getUri() const {
    return &uri_;
}

bool HttpRequest::isWebSocketUpgradeRequest() const {
	if (has(HTTP_SEC_WEBSOCKET_KEY)
		&& has(HTTP_SEC_WEBSOCKET_VERSION)
		&& has(HTTP_CONNECTION)) {
		auto connection = get(HTTP_CONNECTION);
		auto upgrade = get(HTTP_UPGRADE);
		return connection.find("Upgrade") != std::string::npos
			&& upgrade.find(HTTP_WEBSOCKET.str()) != std::string::npos;
	}
	return false;
}

bool HttpRequest::isHttp2UpgradeRequest() const {
	return rapid::utils::caseInsensitiveCompare(get(HTTP_UPGRADE), HTTP_2_0);
}

void HttpRequest::partDataCallback(const char* buffer, size_t size, void* userData) {
	auto httpRequest = reinterpret_cast<HttpRequest*>(userData);
	httpRequest->postFile_.write(buffer, size);
}

HttpResponse::HttpResponse()
	: bufferLen_(0)
	, state_(SEND_HTTP_HEADER)
	, version_(HTTP_1_1)
    , status_("200 OK")
	, statusCode_(HTTP_OK)
	, numberOfBytesToWrite_(0)
	, contentLength_(0) {
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::reset() {
	RAPID_LOG_TRACE() << "Reset HttpResponse state";
    closeFile();
    state_ = SEND_HTTP_HEADER;
	removeAll();
}

void HttpResponse::setKeepAlive(bool keepAlive) {
    if (keepAlive) {
		add(HTTP_CONNECTION, HTTP_KEEP_ALIVE);
    } else {
		add(HTTP_CONNECTION, HTTP_CLOSE);
    }
}

void HttpResponse::setStatusCode(HttpStatusCode code) {
	auto str = fromHttpStatusCode(code);
	status_ = str.code + " " + str.status;
	statusCode_ = code;
}

HttpStatusCode HttpResponse::statusCode() const noexcept {
	return statusCode_;
}

bool HttpResponse::sendStatusPage(rapid::ConnectionPtr &pConn, HttpStatusCode code, HttpRequestPtr httpRequest) {
	auto pSendBuffer = pConn->getSendBuffer();
	std::ostringstream ostr;
	ostr << HttpServerConfigFacade::getInstance().getRootPath() << "/" << static_cast<uint16_t>(code) << ".html";
	wirteToBuffer(pSendBuffer, ostr.str(), httpRequest);
	state_ = SEND_HTTP_CONTENT;
    return false;
}

void HttpResponse::setContentRange(int64_t bytesStart, int64_t bytesEnd, int64_t contentSize) {
	std::ostringstream ostr;
	ostr << "bytes " << bytesStart << "-" << bytesEnd << "/" << contentSize;
	add(HTTP_CONETNT_RANGE, ostr.str());
}

void HttpResponse::closeFile() {
	// 回收filereader
	pFileReader_.reset();
}

void HttpResponse::setContentLength(int64_t contentLength) {
	add(HTTP_CONETNT_LENGTH, contentLength);
}

int64_t HttpResponse::getContentLength() const {
	return contentLength_;
}

void HttpResponse::setBufferLength(uint32_t len) {
	bufferLen_ = len;
}

uint32_t HttpResponse::getBufferLength() const {
	return bufferLen_;
}

void HttpResponse::setRangeResponseHeader(HttpRequestPtr httpRequest) {
	int64_t start = -1;
	int64_t end = -1;
	
	httpRequest->getByteRange(start, end);
	if (end == -1) {
		end = pFileReader_->getFileSize() - 1;
	}
	
	contentLength_ = end + 1 - start;
	setContentRange(start, end, pFileReader_->getFileSize());
	pFileReader_->seekTo(start);
	
	RAPID_LOG_TRACE() << "Range request size: " 
		<< rapid::utils::byteFormat(contentLength_)
		<< "/" 
		<< rapid::utils::byteFormat(pFileReader_->getFileSize());

	numberOfBytesToWrite_ = (std::min)(contentLength_, rapid::SEND_FILE_MAX_SIZE);
	setContentLength(contentLength_);
	setStatusCode(HTTP_PARTICAL_CONTENT);
}

void HttpResponse::setResponseHeader() {
	if (pFileReader_->isOpen()) {
		numberOfBytesToWrite_ = (std::min)(pFileReader_->getFileSize(), rapid::SEND_FILE_MAX_SIZE);
		contentLength_ = pFileReader_->getFileSize();
	} else {
		numberOfBytesToWrite_ = 0;
		contentLength_ = 0;
	}

	remove(HTTP_CONETNT_RANGE);
	setContentLength(contentLength_);
}

void HttpResponse::doSerialize(rapid::IoBuffer* pBuffer) {
	pBuffer->append(version_)
		.append(HTTP_SPACE)
		.append(status_)
		.append(HTTP_CRLF);
	HttpMessage::doSerialize(pBuffer);
}

void HttpResponse::wirteToBuffer(rapid::IoBuffer *pSendBuffer, std::string const &filePath, HttpRequestPtr httpRequest) {
	auto const mimeType = fileExtToMimeType(getFileExt(filePath));
	
	bool compressable = false;
	if (httpRequest->has(HTTP_ACCEPT_ENCODEING)) {
		compressable = isCompressibleable(mimeType);
	}

	pFileReader_ = HttpServerConfigFacade::getInstance().getFileCacheManager().get(filePath, compressable);
	
	if (compressable) {
		add(HTTP_CONETNT_ENCODING, HTTP_GZIP);
	} else {
		remove(HTTP_CONETNT_ENCODING);
	}

	if (HttpServerConfigFacade::getInstance().isUseSSL()) {
		add(HTTP_STRICT_TRANSPORT_SECRUITY, HTTP_MAX_AGE_ONE_YEAR);
	}

	add(HTTP_CONETNT_TYPE, mimeType.contentType);
	add(HTTP_ACCEPT_RANGE, HTTP_BYTES);

	if (httpRequest->isRangeRequest()) {
		setRangeResponseHeader(httpRequest);
		httpRequest->remove(HTTP_RANGE);
	} else {
		setResponseHeader();
	}

	serialize(pSendBuffer);
}

void HttpResponse::writeErrorResponseHeader(rapid::IoBuffer *pSendBuffer, HttpStatusCode errorCode) {
	numberOfBytesToWrite_ = 0;
	contentLength_ = 0;
	setStatusCode(errorCode);
	setContentLength(0);
	serialize(pSendBuffer);
}

void HttpResponse::writeResponseHeader(rapid::ConnectionPtr &pConn, rapid::IoBuffer* pSendBuffer, HttpRequestPtr httpRequest) {
	add(HTTP_SERVER, HttpServerConfigFacade::getInstance().getServerName());
	/*
	if (!httpRequest->has(HTTP_HOST) 
		|| httpRequest->get(HTTP_HOST) != HttpServerConfigFacade::getInstance().getHost()) {
		writeErrorResponseHeader(pSendBuffer, HTTP_BAD_REQUEST);
	}
	*/
	auto pUri = httpRequest->getUri();
	RAPID_LOG_TRACE() << "Request begin " << pUri->path();
	auto const path = pUri->path();

	auto rootPath = HttpServerConfigFacade::getInstance().getRootPath();
	auto filePath(rootPath + path);

	if (path == "/") {
		filePath = rootPath + HttpServerConfigFacade::getInstance().getIndexFileName();
	} else if (path.empty() || !pUri->valid() || !HttpServerConfigFacade::getInstance().getFileCacheManager().isExist(filePath)) {
		RAPID_LOG_TRACE() << "Request end " << pUri->path();
		sendStatusPage(pConn, HTTP_NOT_FOUND, httpRequest);
		return;
	} 

	setStatusCode(HTTP_OK);
	wirteToBuffer(pSendBuffer, filePath, httpRequest);
	state_ = SEND_HTTP_CONTENT;
}

bool HttpResponse::send(rapid::ConnectionPtr &pConn, HttpRequestPtr httpRequest) {
	auto pSendBuffer = pConn->getSendBuffer();
	setBufferLength(pSendBuffer->goodSize());

	switch (state_) {
	case SEND_HTTP_HEADER:
		setKeepAlive(true);
		writeResponseHeader(pConn, pSendBuffer, httpRequest);
		break;
	case SEND_HTTP_CONTENT:
		return writeContent(pSendBuffer);
		break;
	}
	return false;
}

bool HttpResponse::writeContent(rapid::IoBuffer *pSendBuffer) {
	RAPID_LOG_TRACE_FUNC();
	state_ = SEND_HTTP_CONTENT;
	// 如果是HTTPs無法一次傳送很大的buffer, 因為加密後的緩衝區會變大
	//auto bytesRead = pSendBuffer->writeable();
	auto bytesRead = (std::max)(pSendBuffer->size() - pSendBuffer->getWrittenSize(),
		pSendBuffer->goodSize());
	return pFileReader_->read(pSendBuffer, bytesRead);
}
