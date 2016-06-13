//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

enum HttpStatusCode : unsigned short {
	// Informational
	HTTP_CONTUNUE = 100,
	HTTP_SWITCHING_PROTOCOLS = 101,
	// Successful
	HTTP_OK = 200,
	HTTP_ACCEPTED = 202,
	HTTP_NO_CONTENT = 204,
	HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
	HTTP_PARTICAL_CONTENT = 206,
	// Redirection
	HTTP_AMBIGUOUS = 300,
	HTTP_MOVED = 301,
	HTTP_FOUND = 302,
	HTTP_NOT_MODIFED = 304,
	// Client Error
	HTTP_BAD_REQUEST = 400,
	HTTP_FORBIDDEN = 403,
	HTTP_NOT_FOUND = 404,
	HTTP_NOT_ALLOWED = 405,
	HTTP_NOT_ACCEPTABLE = 406,
	HTTP_CONFLICT = 409,
	HTTP_GONE = 410,
	HTTP_LENGTH_REQUIRED = 411,
	HTTP_EXPECTATION_FAILED = 417,
	// Server Error
	HTTP_INTERNAL_SERVER_ERROR = 500,
	HTTP_NO_IMPLMENTED = 501,
	HTTP_BAD_GATEWAY = 502,
	HTTP_GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
};

struct HttpStatus {
	std::string code;
	std::string status;
};
