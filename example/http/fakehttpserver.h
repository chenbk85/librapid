//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <filesystem>
#include <condition_variable>

#include <rapid/connection.h>
#include <rapid/tcpserver.h>

#include "messagedispatcher.h"

class FakeHttpServer {
public:
	static std::unique_ptr<FakeHttpServer> createHttpServer(std::string const &ipAddress, unsigned short port);

	FakeHttpServer(std::string const &ipAddress, unsigned short port);

	~FakeHttpServer();

	FakeHttpServer(FakeHttpServer const &) = delete;
	FakeHttpServer& operator=(FakeHttpServer const &) = delete;

	void start();

	void stop();

private:
	enum {
		TCP_BUFFER_SIZE = 64 * 1024
	};

	void onNewConnection(rapid::ConnectionPtr pConn);

	rapid::TcpServer server_;
	std::mutex waitStopMutex_;
	std::condition_variable stopFlag_;
};