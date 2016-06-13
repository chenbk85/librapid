//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <rapid/platform/platform.h>
#include <rapid/details/socket.h>

#include <rapid/details/timingwheel.h>
#include <rapid/eventhandler.h>
#include <rapid/connection.h>

namespace rapid {

class BlockFactory;

namespace details {

class IoEventDispatcher;

class SocketAcceptPooller {
public:
    SocketAcceptPooller(std::shared_ptr<TcpServerSocket> &listenSocket,
                     std::shared_ptr<BlockFactory> &factory);

    SocketAcceptPooller(SocketAcceptPooller const &) = delete;
    SocketAcceptPooller& operator=(SocketAcceptPooller const &) = delete;

    ~SocketAcceptPooller();

    void setPoolSize(size_t size);

    void setScaleSize(size_t size);

	void startPollAcceptEvent();

    void stopPoll();

    void setContextEventHandler(ContextEventHandler &&callback);

private:
	static auto constexpr DEFAULT_POOL_SIZE = 100;
	static auto constexpr POLL_NETWORK_EVENT_TIMEOUT = 15000;

	void createSocket(std::vector<ConnectionPtr> &newConnList);
	
	bool hasAcceptConnection(WSANETWORKEVENTS *events) const;
    
	void addSocketToPool();
    
	void pollNetworkEvent();

    void pollLoop();

    ContextEventHandler contextCallback_;
    std::vector<ConnectionPtr> connPool_;
    std::shared_ptr<details::TcpServerSocket> pListenSocket_;
	TimingWheelPtr pReuseTimingWheel_;
    std::shared_ptr<BlockFactory> pBlockFactory_;
    std::thread pollerThread_;
    HANDLE postMoreAcceptEvent_;
    HANDLE shutdownEvent_;
    size_t maxPoolSize_;
    size_t scaleSize_;
};

}

}
