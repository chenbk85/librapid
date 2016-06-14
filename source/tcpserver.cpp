//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <iomanip>

#include <rapid/platform/threadutils.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/tcpipparameters.h>

#include <rapid/utils/singleton.h>
#include <rapid/utils/stringutilis.h>

#include <rapid/details/socket.h>
#include <rapid/details/contracts.h>
#include <rapid/details/socketacceptpoller.h>
#include <rapid/details/wasextapi.h>
#include <rapid/details/ioeventdispatcher.h>
#include <rapid/details/iothreadpool.h>
#include <rapid/details/socketaddress.h>
#include <rapid/details/blockfactory.h>

#include <rapid/logging/logging.h>
#include <rapid/tcpserver.h>

namespace rapid {

TcpServer::TcpServer(uint16_t localPort)
    : TcpServer("0.0.0.0", localPort) {
}

TcpServer::TcpServer(std::string const &localAddress, uint16_t localPort)
    : TcpServer(localAddress, localPort, 1) {
}

TcpServer::TcpServer(std::string const &localAddress, uint16_t localPort, uint32_t threadPerCpu)
    : localAddress_(localAddress)
	, localPort_(localPort)
    , numThreadPerCpu_(threadPerCpu)
	, numNumaNode_(0) {
	RAPID_ENSURE(platform::startupWinSocket());
}

TcpServer::~TcpServer() {
	shutdown();
}

void TcpServer::startListening(ContextEventHandler &&callback, uint16_t numaNode) {
	RAPID_LOG_TRACE_INFO();

	numNumaNode_ = numaNode;

	auto const sockName = details::SocketAddress::getSockName(pListenSocket_->socketFd());

	RAPID_LOG_INFO() << "Server is listening on "
		<< sockName.addressToString()
		<< " port "
		<< sockName.port();

	details::WsaExtAPI::getInstance().setSkipIoSyncNotify(*pListenSocket_);
	
	setProcessAffinity();

	startThreadPool();
	
	if (!pSocketAcceptPooller_) {
		setSocketPool(100, 100, 4096);
	}
	
	pSocketAcceptPooller_->setContextEventHandler(std::forward<ContextEventHandler>(callback));
	pSocketAcceptPooller_->startPoll();
}

void TcpServer::shutdown() {
    RAPID_LOG_INFO() << "Shutting down server...";

    if (pThreadPool_ != nullptr) {
        pThreadPool_->joinAll();
    }

    if (pSocketAcceptPooller_ != nullptr) {
		pSocketAcceptPooller_->stopPoll();
    }
    pThreadPool_.reset();
	pSocketAcceptPooller_.reset();
    pListenSocket_.reset();
}

void TcpServer::startThreadPool() {
	RAPID_LOG_TRACE_INFO();

	uint32_t concurrentThreadCount = 0;

	if (platform::SystemInfo::getInstance().isNumaSystem()) {
		auto const processorInfo = platform::SystemInfo::getInstance().getProcessorInformation();
		concurrentThreadCount = processorInfo.processorCoreCount * numThreadPerCpu_;
	}
	else {
		concurrentThreadCount = platform::SystemInfo::getInstance().getNumberOfProcessors(numThreadPerCpu_);
	}

	details::IoEventDispatcher dispatcher(concurrentThreadCount);
	// 保存IoEventDispatcher
	details::IoEventDispatcher::getInstance() = std::move(dispatcher);
	pThreadPool_ = std::make_shared<details::IoThreadPool>(concurrentThreadCount);

	pThreadPool_->runAll();

	uint32_t i = 0;

	for (auto const &thread : pThreadPool_->threads()) {
		auto pThread = const_cast<std::thread *>(&thread);
		if (!platform::SystemInfo::getInstance().isNumaSystem()) {
			RAPID_LOG_INFO() << "IO Worker thread " << i + 1
				<< "(" << std::setw(5) << thread.get_id() << ")"
				<< " starting...";
		}
		else {
			RAPID_LOG_INFO() << "IO Worker thread " << i + 1
				<< "(" << std::setw(5) << thread.get_id() << "Numa: " << numNumaNode_ << ")"
				<< " starting...";
		}
		++i;
	}
}

void TcpServer::setProcessAffinity() const {
	RAPID_LOG_TRACE_INFO();

	platform::setProcessPriorityBoost(true);

	if (!platform::SystemInfo::getInstance().isNumaSystem()) {
		// 開啟動態更新處理器親和性!
		if (!::SetProcessAffinityUpdateMode(::GetCurrentProcess(), PROCESS_AFFINITY_ENABLE_AUTO_UPDATE)) {
			throw Exception();
		}
	}
	else {
		for (auto const &numaNode : platform::SystemInfo::getInstance().getNumaProcessorInformation()) {
			if (numNumaNode_ == numaNode.node) {
				if (!::SetProcessAffinityMask(::GetCurrentProcess(), numaNode.processorMask)) {
					throw Exception();
				}
				break;
			}
		}
	}
}

void TcpServer::setSocketPool(uint16_t scaleSocketSize, uint16_t poolSocketSize, size_t bufferSize) {
	RAPID_LOG_TRACE_INFO();

	pListenSocket_ = std::make_shared<details::TcpServerSocket>(localAddress_, localPort_, SOMAXCONN);
	
	auto const roundPageSize = platform::SystemInfo::getInstance().roundUpToPageSize(bufferSize);
    auto const maxPageCount = poolSocketSize * 2; // Read and write
    auto const allocPoolSize = maxPageCount * roundPageSize;
    
	auto pBlockFactory = details::BlockFactory::createBlockFactory(numNumaNode_, maxPageCount, roundPageSize);

    RAPID_LOG_INFO() << "Pool size=" << utils::byteFormat(allocPoolSize, 1)
                     << "(page=" << allocPoolSize / SYSTEM_PAGE_SIZE
					 << ", size=" << utils::byteFormat(roundPageSize, 0)
                     << ")";
    
	pSocketAcceptPooller_ = std::make_shared<details::SocketAcceptPooller>(pListenSocket_, pBlockFactory);
	pSocketAcceptPooller_->setPoolSize(poolSocketSize);
	pSocketAcceptPooller_->setScaleSize(scaleSocketSize);
}

}
