//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include "httpheaders.h"

// HTTP headers define
HttpHeaderName const HTTP_CONETNT_ENCODING("Content-Encoding");
HttpHeaderName const HTTP_CONETNT_TYPE("Content-Type");
HttpHeaderName const HTTP_CONTENT_DISPOSITION("Content-Disposition");
HttpHeaderName const HTTP_CONETNT_LENGTH("Content-Length");
HttpHeaderName const HTTP_CONETNT_RANGE("Content-Range");
HttpHeaderName const HTTP_ACCEPT_RANGE("Accept-Ranges");
HttpHeaderName const HTTP_RANGE("Range");
HttpHeaderName const HTTP_CONNECTION("Connection");
HttpHeaderName const HTTP_SERVER("Server");
HttpHeaderName const HTTP_STRICT_TRANSPORT_SECRUITY("Strict-Transport-Security");
HttpHeaderName const HTTP_HOST("Host");
HttpHeaderName const HTTP_ACCEPT_ENCODEING("Accept-Encoding");
HttpHeaderName const HTTP_UPGRADE("Upgrade");

HttpHeaderName const HTTP2_SETTINGS { "HTTP2-Settings" };
std::string const HTTP2_UPGRADE_PROTOCOL { "h2c" };
std::string const HTTP_2_0 { "HTTP/2.0" };
std::string const HTTP_1_1 { "HTTP/1.1" };

std::string const HTTP_MULTIPART_FORM_DATA{ "multipart/form-data" };
std::string const HTTP_APP_X_WWW_FORM_URLENCODED{ "application/x-www-form-urlencoded" };
std::string const HTTP_BOUNDARY_EQ{ "boundary=" };

// Websocket header define
HttpHeaderName const HTTP_WEBSOCKET { "websocket" };
HttpHeaderName const HTTP_SEC_WEBSOCKET_KEY { "Sec-WebSocket-Key" };
HttpHeaderName const HTTP_SEC_WEBSOCKET_EXTENSIONS { "Sec-WebSocket-Extensions" };
HttpHeaderName const HTTP_SEC_WEBSOCKET_VERSION { "Sec-WebSocket-Version" };
HttpHeaderName const HTTP_SEC_WEBSOCKET_PROTOCOL { "Sec-WebSocket-Protocol" };
HttpHeaderName const HTTP_SEC_WEBSOCKET_ACCEPT { "Sec-WebSocket-Accept" };

std::string const HTTP_METHOD_GET { "GET" };
std::string const HTTP_METHOD_POST { "POST" };

std::string const HTTP_CRLF { "\r\n" };
std::string const HTTP_SPACE{ " " };
std::string const HTTP_GZIP { "gzip" };
std::string const HTTP_BYTES{ "bytes" };
std::string const HTTP_MAX_AGE_ONE_YEAR{ "max-age=31536000" };
std::string const HTTP_KEEP_ALIVE{ "Keep-Alive" };
std::string const HTTP_CLOSE{ "close" };

uint32_t constexpr HTTP_HEADER_MAX = 100;
