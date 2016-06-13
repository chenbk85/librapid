//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <exception>

#include "httpstatuscode.h"

class HttpRequestErrorException : public std::exception {
public:
	explicit HttpRequestErrorException(HttpStatusCode code)
		: std::exception("Bad http request")
		, errorCode_(code) {
	}

	HttpStatusCode errorCode() const {
		return errorCode_;
	}
private:
	HttpStatusCode errorCode_;
};

class HttpMessageNotFoundException : public std::exception {
public:
	HttpMessageNotFoundException()
		: std::exception("Not found http header") {
	}
};

class MalformedDataException : public std::exception {
public:
	MalformedDataException()
		: std::exception("Malformed data") {
	}
};

class NotImplementedException : public std::exception {
public:
	NotImplementedException()
		: std::exception("Not implemented") {
	}
};
