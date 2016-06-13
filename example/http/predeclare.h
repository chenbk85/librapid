//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

class WebSocketService;
using WebSocketServicePtr = std::shared_ptr<WebSocketService>;

class HttpRequest;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;

class HttpResponse;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

class WebSocketRequest;
using WebSocketRequestPtr = std::shared_ptr<WebSocketRequest>;

class HttpContext;
using HttpContextPtr = std::shared_ptr<HttpContext>;

class HttpsContext;
using HttpsContextPtr = std::shared_ptr<HttpsContext>;

class HttpFileReader;
using HttpFileReaderPtr = std::shared_ptr<HttpFileReader>;


