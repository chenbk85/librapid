//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/logging/logging.h>
#include <rapid/iobuffer.h>

#include "fakehttpserver.h"

std::unique_ptr<FakeHttpServer> FakeHttpServer::createHttpServer(std::string const &ipAddress, unsigned short port) {
	return std::make_unique<FakeHttpServer>(ipAddress, port);
}

FakeHttpServer::FakeHttpServer(std::string const &ipAddress, unsigned short port)
	: server_(ipAddress, port) {
	server_.setSocketPool(1000, 20000, 16 * 1024);
}

FakeHttpServer::~FakeHttpServer() {

}

void FakeHttpServer::stop() {
	stopFlag_.notify_one();
}

void FakeHttpServer::onNewConnection(rapid::ConnectionPtr pConn) {
	static const std::string s_httpReplyContent{
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 100\r\n"
		"Connection: keep-alive\r\n"
		"Host: 127.0.0.1\r\n"
		"\r\n"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
	};

	auto pRecvBuffer = pConn->getReceiveBuffer();
	auto pSendBuffer = pConn->getSendBuffer();

	do {
		pRecvBuffer->retrieve(pRecvBuffer->readable());

		pSendBuffer->append(s_httpReplyContent);
		if (!pConn->sendAsync()) {
			break;
		}
	} while (pRecvBuffer->readSome(pConn));
}

void FakeHttpServer::start() {
	server_.startListening([this](rapid::ConnectionPtr pConn) {
		pConn->setSendEventHandler([this](rapid::ConnectionPtr conn) {
			onNewConnection(conn);
		});

		pConn->setReceiveEventHandler([this](rapid::ConnectionPtr conn) {
			onNewConnection(conn);
		});

		pConn->setAcceptEventHandler([this](rapid::ConnectionPtr conn) {
			onNewConnection(conn);
		});
	});
	RAPID_LOG_INFO() << "HttpServer starting...";
	RAPID_LOG_INFO() << "Press 'Ctrl+C' to stop";
	std::unique_lock<std::mutex> lock(waitStopMutex_);
	stopFlag_.wait(lock);
	server_.shutdown();
}
