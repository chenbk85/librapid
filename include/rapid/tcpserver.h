//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <memory>

#include <rapid/eventhandler.h>

namespace rapid {

namespace details {
class IoEventDispatcher;
class IoThreadPool;
class SocketAcceptPooller;
class TcpServerSocket;
class BlockFactory;
}

class TcpServer {
public:
    explicit TcpServer(uint16_t localPort);

    TcpServer(std::string const &localAddress, uint16_t localPort);

    TcpServer(std::string const &localAddress, uint16_t localPort, uint32_t threadPerCpu);

    ~TcpServer();

	void startListening(ContextEventHandler &&callback, uint16_t numaNode = 0);

    void shutdown();

    void setSocketPool(uint16_t scaleSocketSize, uint16_t poolSocketSize, size_t bufferSize);

private:
	static uint32_t constexpr SYSTEM_PAGE_SIZE = 64 * 1024;

    void setProcessAffinity() const;

    void startThreadPool();
	
	uint16_t localPort_;
	uint16_t numNumaNode_;
	uint32_t numThreadPerCpu_;
	std::string localAddress_;
    std::shared_ptr<details::TcpServerSocket> pListenSocket_;
    std::shared_ptr<details::SocketAcceptPooller> pSocketAcceptPooller_;
    std::shared_ptr<details::IoThreadPool> pThreadPool_;
};

}
