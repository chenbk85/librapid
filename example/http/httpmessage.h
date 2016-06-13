//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <map>
#include <fstream>

#include <MultipartReader.h>

#include <rapid/utils/stringutilis.h>

#include "uri.h"
#include "httpexception.h"
#include "messagedispatcher.h"
#include "httpheaders.h"
#include "httpconstants.h"
#include "predeclare.h"

class HttpMessage : public MessageContext {
public:
	HttpMessage() = default;

	virtual ~HttpMessage() = default;

	void add(std::string const &name, std::string const &value) {
		headers_.add(name, value);
	}

	void add(HttpHeaderName const &header, std::string const &value) {
		headers_.add(header, value);
	}

	void add(HttpHeaderName const &header, int64_t value) {
		headers_.add(header, std::to_string(value));
	}

	void remove(HttpHeaderName const &header) {
		headers_.remove(header);
	}

	std::string const & get(HttpHeaderName const &header) const {
		auto const& value = headers_.get(header);
		if (!value.empty()) {
			return value;
		}
		throw HttpMessageNotFoundException();
	}

	void removeAll() {
		headers_.clear();
	}

	bool has(HttpHeaderName const &header) const noexcept;

	friend std::ostream& operator<<(std::ostream &ostr, HttpMessage const &message) {
		message.headers_.foreach([&](std::string const &name, std::string const &value) {
			ostr << name << ": " << value << "\n";
		});
		return ostr;
	}

	void setVersion(int version) noexcept {
		version_ = version;
	}

	int version() const noexcept {
		return version_;
	}
protected:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;
	virtual void doUnserialize(rapid::IoBuffer *pBuffer) override;
	int version_;
	HttpHeaders headers_;
};

__forceinline bool HttpMessage::has(HttpHeaderName const &header) const noexcept {
	return headers_.has(header);
}

class HttpRequest : public HttpMessage {
public:
    HttpRequest();

    virtual ~HttpRequest();

    void setUri(std::string const &str);

    std::string method() const;

    void setMethod(std::string const &str);

	void setUri(char const *str, size_t length);

    uint64_t getContentLength() const;

    void createTempFile();

    void closePostFile();

    bool writeTempFile(rapid::ConnectionPtr pConn);

    void setKeepAlive();

    bool isKeepAlive() const;

	bool isRangeRequest() const;

	void getByteRange(int64_t &start, int64_t &end) const;

	Uri const * getUri() const;

	bool isUpgradeRequest() const noexcept;

	bool isWebSocketUpgradeRequest() const;

	bool isHttp2UpgradeRequest() const;

private:
	static void partDataCallback(const char *buffer, size_t size, void *userData);
	
	void parseMultipartFormData(char const *buf, size_t bufSize);

	bool keepAlive_ : 1;
	Uri uri_;
	MultipartReader multipartReader_;
    std::string method_;
    std::ofstream postFile_;
	size_t totalReadPostSize_;
};

__forceinline bool HttpRequest::isUpgradeRequest() const noexcept {
	return has(HTTP_UPGRADE);
}

class HttpResponse : public HttpMessage {
public:
    enum SendState {
        SEND_HTTP_HEADER,
        SEND_HTTP_CONTENT,
    };

    HttpResponse();

    virtual ~HttpResponse();

	void reset();

    void setKeepAlive(bool keepAlive);

    void setStatusCode(HttpStatusCode code);

	HttpStatusCode statusCode() const noexcept;

	void writeErrorResponseHeader(rapid::IoBuffer *pSendBuffer, HttpStatusCode errorCode);

	bool sendStatusPage(rapid::ConnectionPtr &pConn, HttpStatusCode code, HttpRequestPtr httpRequest);

	virtual bool send(rapid::ConnectionPtr &pConn, HttpRequestPtr httpRequest);

	virtual void writeResponseHeader(rapid::ConnectionPtr &pConn, rapid::IoBuffer* pSendBuffer, HttpRequestPtr httpRequest);

	virtual bool writeContent(rapid::IoBuffer *pSendBuffer);

	void setContentRange(int64_t bytesStart, int64_t bytesEnd, int64_t contentSize);

	void setContentLength(int64_t contentLength);

	int64_t getContentLength() const;

	void setBufferLength(uint32_t len);

	uint32_t getBufferLength() const;

private:
	void wirteToBuffer(rapid::IoBuffer *pSendBuffer, std::string const &filePath, HttpRequestPtr httpRequest);

	virtual void doSerialize(rapid::IoBuffer *pBuffer) override;

	void setRangeResponseHeader(HttpRequestPtr pReq);
	
	void setResponseHeader();

	void closeFile();

	uint32_t bufferLen_;
	SendState state_;
    std::string version_;
    std::string status_;
	HttpStatusCode statusCode_;
	HttpFileReaderPtr pFileReader_;
	int64_t numberOfBytesToWrite_;
	int64_t contentLength_;
};

