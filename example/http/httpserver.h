//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <filesystem>
#include <condition_variable>

#include <rapid/platform/srwlock.h>
#include <rapid/platform/spinlock.h>
#include <rapid/details/timingwheel.h>
#include <rapid/tcpserver.h>

#include "messagedispatcher.h"
#include "predeclare.h"
#include "httpcontext.h"

class HttpServer {
public:
	static std::unique_ptr<HttpServer> createHttpServer(std::string const &ipAddress, uint16_t port);

	HttpServer(std::string const &ipAddress, uint16_t port);

	~HttpServer();

    HttpServer(HttpServer const &) = delete;
    HttpServer& operator=(HttpServer const &) = delete;

	void start();

	void stop();

private:
	void onNewConnection(rapid::ConnectionPtr &pConn);
	
	HttpContextPtr insertHttpContext(size_t id);
	
	HttpContextPtr removeHttpContext(size_t id);

	static HttpContextPtr createHttpContext();
    
	rapid::TcpServer server_;
	mutable rapid::platform::Spinlock lock_;
	std::mutex waitStopMutex_;
	std::condition_variable stopFlag_;
	std::unordered_map<size_t, HttpContextPtr> contextMap_;
};