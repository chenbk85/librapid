//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <iomanip>

#include <rapid/exception.h>
#include <rapid/details/blockfactory.h>
#include <rapid/details/socketaddress.h>

#include <rapid/utils/scopeguard.h>

#include <rapid/logging/logging.h>

#include <rapid/details/socket.h>
#include <rapid/details/contracts.h>
#include <rapid/details/ioeventdispatcher.h>
#include <rapid/details/wasextapi.h>

#include <rapid/platform/privilege.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/threadutils.h>
#include <rapid/platform/tcpipparameters.h>

#include <rapid/details/socketacceptpoller.h>

namespace rapid {

namespace details {

static void defaultCreateContextCallback(ConnectionPtr) {
}

SocketAcceptPooller::SocketAcceptPooller(std::shared_ptr<TcpServerSocket> &listenSocket,
                                   std::shared_ptr<BlockFactory> &factory)
    : contextCallback_(defaultCreateContextCallback)
    , pListenSocket_(listenSocket)
    , pBlockFactory_(factory)
    , postMoreAcceptEvent_(nullptr)
    , shutdownEvent_(nullptr)
    , maxPoolSize_(DEFAULT_POOL_SIZE)
    , scaleSize_(DEFAULT_POOL_SIZE) {
}

SocketAcceptPooller::~SocketAcceptPooller() {
}

void SocketAcceptPooller::setPoolSize(size_t size) {
    RAPID_ENSURE(size > 0);
	maxPoolSize_ = (std::min)(size, static_cast<size_t>(platform::TcpIpParameters::getInstance().getMaxUserPort()));
}

void SocketAcceptPooller::setScaleSize(size_t size) {
	scaleSize_ = (std::min)(static_cast<size_t>(platform::TcpIpParameters::getInstance().getMaxUserPort()), size);
}

void SocketAcceptPooller::setContextEventHandler(ContextEventHandler &&callback) {
	contextCallback_ = std::forward<ContextEventHandler>(callback);
}

void SocketAcceptPooller::stopPoll() {
    if (shutdownEvent_ != nullptr) {
        ::SetEvent(shutdownEvent_);
    }
    
	if (pollerThread_.joinable()) {
        pollerThread_.join();
    }
    
	if (shutdownEvent_ != nullptr) {
        ::CloseHandle(shutdownEvent_);
        shutdownEvent_ = nullptr;
    }
    
	if (postMoreAcceptEvent_ != nullptr) {
        ::CloseHandle(postMoreAcceptEvent_);
        postMoreAcceptEvent_ = nullptr;
    }

	pReuseTimingWheel_.reset();

    RAPID_LOG_INFO() << "Cancel all pending request";
    for (auto &pConn : connPool_) {
        pConn->cancelPendingRequest();
    }
    
	utils::Singleton<WsaExtAPI>::getInstance().cancelAllPendingIoRequest(*pListenSocket_);
}

void SocketAcceptPooller::startPoll() {
	RAPID_LOG_TRACE_INFO();

    RAPID_ENSURE(scaleSize_ <= maxPoolSize_);
    
    shutdownEvent_ = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	auto shutdownEventGuard = utils::makeScopeGurad([this]() {
		::CloseHandle(shutdownEvent_);
		shutdownEvent_ = nullptr;
	});

    if (!shutdownEvent_) {
        throw Exception();
    }

    postMoreAcceptEvent_ = ::WSACreateEvent();
	auto postMoreAcceptEventGuard = utils::makeScopeGurad([this]() {
		::CloseHandle(postMoreAcceptEvent_);
		postMoreAcceptEvent_ = nullptr;
	});

    if (!postMoreAcceptEvent_) {
        throw Exception();
    }

    if (::WSAEventSelect(pListenSocket_->socketFd(), postMoreAcceptEvent_, FD_ACCEPT) < 0) {
        throw Exception();
    }

	// 建立TIME-WAIT狀態的timer(系統設定)
	pReuseTimingWheel_ = TimingWheel::createTimingWheel(1000 
		* platform::TcpIpParameters::getInstance().getTcpTimedWaitDelay(), 60);
	pReuseTimingWheel_->start();

	details::IoEventDispatcher::getInstance().addDevice(pListenSocket_->handle(), 0);

    addSocketToPool();

    pollerThread_ = std::thread(std::bind(&SocketAcceptPooller::pollLoop, this));
   
	platform::setThreadName(&pollerThread_, "SocketAcceptPooller Thread");
    
	RAPID_LOG_INFO() << "SocketAcceptPooller thread("
                     << std::setw(5) << pollerThread_.get_id()
                     << ") starting...";
	
	shutdownEventGuard.dismiss();
	postMoreAcceptEventGuard.dismiss();
}

void SocketAcceptPooller::addSocketToPool() {
    auto prepareSize = maxPoolSize_ - connPool_.size();
    if (!prepareSize) {
        RAPID_LOG_INFO() << "Upper limit on socket pool size!";
        return;
    }

    std::vector<ConnectionPtr> connections;
	connections.reserve(scaleSize_);
    
	for (auto i = prepareSize; i < prepareSize + scaleSize_; ++i) {
		createSocket(connections);
    }

	connPool_.reserve(connPool_.size() + connections.size());
	connPool_.insert(connPool_.end(), connections.begin(), connections.end());

	RAPID_LOG_INFO() << "Background post " << connections.size() << " pre-accepted socket ";
}

void SocketAcceptPooller::createSocket(std::vector<ConnectionPtr> &newConnList) {
    try {
        auto pConn = std::make_shared<Connection>(pListenSocket_.get(), *pBlockFactory_, pReuseTimingWheel_);
		contextCallback_(pConn);        
		pConn->acceptAsync();
        newConnList.push_back(pConn);
	} catch (Exception const &e) {
        RAPID_LOG_FATAL() << e.error() << " " << e.what();
    } catch (ExceptionWithMinidump const &e) {
        RAPID_LOG_FATAL() << e.what();
	} catch (std::exception const &e) {
		RAPID_LOG_FATAL() << e.what();
	}
}

bool SocketAcceptPooller::hasAcceptConnection(WSANETWORKEVENTS *events) const {
    auto retval = ::WSAEnumNetworkEvents(pListenSocket_->socketFd(), postMoreAcceptEvent_, events);
	if (retval < 0) {
        throw Exception();
    }

    return ((events->lNetworkEvents & FD_ACCEPT) && (events->iErrorCode[FD_ACCEPT_BIT] == 0));
}

void SocketAcceptPooller::pollLoop() {
    try {
        pollNetworkEvent();
    } catch (Exception const &e) {
		RAPID_LOG_FATAL() << e.error() << " " << e.what();
    } catch (ExceptionWithMinidump const &e) {
		RAPID_LOG_FATAL() << e.what();
	} catch (std::exception const &e) {
		RAPID_LOG_FATAL() << e.what();
	}
    RAPID_LOG_INFO() << "AcceptSocketPool thread stopping...";
}

void SocketAcceptPooller::pollNetworkEvent() {
	RAPID_LOG_TRACE_INFO();

    HANDLE const waitEvents[2] = {
        shutdownEvent_,
        postMoreAcceptEvent_
    };

    WSANETWORKEVENTS events;
    
	for (;;) {
        auto waitObjects = ::WSAWaitForMultipleEvents(_countof(waitEvents),
													  waitEvents,
													  FALSE,
													  POLL_NETWORK_EVENT_TIMEOUT,
													  FALSE);
        
		switch (waitObjects) {
        // Shutdown event.
        case WAIT_OBJECT_0:
            return;
            break;
        // Post more pre-accept connection.
        case WAIT_OBJECT_0 + 1:
			if (hasAcceptConnection(&events)) {
                addSocketToPool();
                ::WSAResetEvent(waitEvents[1]);
            }
            break;
		case WAIT_TIMEOUT:
			break;
        default:
            throw Exception();
        }
    }
}

}

}
