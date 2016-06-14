//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/tcpipparameters.h>

#include <rapid/logging/logging.h>

#include <rapid/details/timingwheel.h>
#include <rapid/details/contracts.h>
#include <rapid/details/ioeventdispatcher.h>
#include <rapid/details/wasextapi.h>

#include <rapid/iobuffer.h>
#include <rapid/connection.h>

namespace rapid {

static void defaultAcceptConnection(ConnectionPtr pConn) {
    pConn->sendAndDisconnec();
}

static void defaultRecvComplete(ConnectionPtr) {
}

static void defaultSendComplete(ConnectionPtr) {
}

static void defaultDisconnectComplete(ConnectionPtr) {
}

static void setSocketOption(details::TcpSocket &pSocket) {
	// Enable TCP_NODELAY
	static BOOL const optValue = TRUE;
	if (pSocket.setSockOpt(IPPROTO_TCP, TCP_NODELAY, &optValue) < 0) {
		throw Exception();
	}

	int sendBufferSize = 0;
	if (pSocket.setSockOpt(SOL_SOCKET, SO_SNDBUF, &sendBufferSize) < 0) {
		throw Exception();
	}
    
	// Make the socket non-inheritable
    if (!::SetHandleInformation(reinterpret_cast<HANDLE>(pSocket.socketFd()), HANDLE_FLAG_INHERIT, 0)) {
        throw Exception();
    }

    if (::IsWindows8OrGreater()) {
        if (!pSocket.ioctl(SIO_LOOPBACK_FAST_PATH)) {
            throw Exception();
        }
    }

	details::WsaExtAPI::getInstance().setSkipIoSyncNotify(pSocket);
}

Connection::AcceptBufferSize::AcceptBufferSize() noexcept
	: AcceptBufferSize(IPV6_ACCEPT_SOCKADDR_SIZE) {
}

Connection::AcceptBufferSize::AcceptBufferSize(uint32_t acceptBufferSize) noexcept
	: bufferSize(acceptBufferSize)
	, recvLen(acceptBufferSize - IPV6_ACCEPT_SOCKADDR_SIZE) {
}

Connection::Connection(details::TcpServerSocket *listenSocket, details::BlockFactory &factory, details::TimingWheelPtr reuseTimingWheel)
	: isReuseSocket_(false)
	, hasUpdataAcceptContext_(true)
	, isSendShutdown_(false)
	, isRecvShutdown_(false)
	, lastOptFlags_(details::IOFlags::IO_ACCEPT_PENDDING)
	, halfClosedState_(ACTIVE_CLOSE)
	, pListenSocket_(listenSocket)
	, pTimeWaitReuseTimer_(reuseTimingWheel)
	, pAcceptBuffer_(std::make_unique<IoBuffer>())
    , pSendBuffer_(std::make_unique<IoBuffer>(0, factory))
	, pReceiveBuffer_(std::make_unique<IoBuffer>(0, factory))
	, pPostBuffer_(std::make_unique<IoBuffer>())
	, pDisconnectBuffer_(std::make_unique<IoBuffer>())
	, acceptSize_(pReceiveBuffer_->size()) {
	pAcceptBuffer_->setCompleteHandler(defaultAcceptConnection);
	pSendBuffer_->setCompleteHandler(defaultSendComplete);
	pReceiveBuffer_->setCompleteHandler(defaultRecvComplete);
	pDisconnectBuffer_->setCompleteHandler(defaultDisconnectComplete);
	// 實驗性質: 需要搭配SE_LOCK_MEMORY_NAME
#ifdef ENABLE_LOCK_MEMORY
	//pReceiveBuffer_->lockOverlappedRange(acceptSocket_);
	//pSendBuffer_->lockOverlappedRange(acceptSocket_);
#endif
	resetOverlappedValue();
    setSocketOption(acceptSocket_);
}

Connection::~Connection() {
}

void Connection::updateAcceptContext() {
    auto listenSocket = pListenSocket_->socketFd();
    if (acceptSocket_.setSockOpt(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &listenSocket) < 0) {
        throw Exception();
    }

    hasUpdataAcceptContext_ = true;
    if (!isReuseSocket_) {
		details::IoEventDispatcher::getInstance().addDevice(acceptSocket_.handle(), reinterpret_cast<ULONG_PTR>(this));
    }
}

void Connection::postAsync(std::function<void(ConnectionPtr)> handler) {
	RAPID_LOG_TRACE_FUNC();
	pPostBuffer_->setCompleteHandler(handler);
	pPostBuffer_->ioFlag = details::IOFlags::IO_POST_PENDDING;
	details::IoEventDispatcher::getInstance().post(reinterpret_cast<ULONG_PTR>(this), pPostBuffer_.get());
}

bool Connection::acceptAsync() {
    RAPID_ENSURE(hasUpdataAcceptContext_ == true);
    
	hasUpdataAcceptContext_ = false;
	lastOptFlags_ = details::IOFlags::IO_ACCEPT_PENDDING;
	halfClosedState_ = ACTIVE_CLOSE;
    pSendBuffer_->reset();
    pReceiveBuffer_->reset();
	
	auto tryToAccepNewConn = true;

	for (; tryToAccepNewConn;) {

		resetOverlappedValue();
		DWORD bytesRead = 0;

		auto retval = details::WsaExtAPI::getInstance().acceptEx(*pListenSocket_,
			acceptSocket_,
			pReceiveBuffer_->begin(),
			acceptSize_.recvLen,
			acceptSize_.localAddrLen,
			acceptSize_.remoteAddrLen,
			&bytesRead,
			this);

		if (retval) {
			lastOptFlags_ = details::IOFlags::IO_ACCEPT_COMPLETED;
			return true;
		}

		auto lastError = ::GetLastError();
		switch (lastError) {
			// These happen if connection reset is received before AcceptEx could complete. 
			// These errors relate to new connection, not to AcceptEx, so ignore broken connection and
			// try AcceptEx again for more connections.
		case ERROR_NETNAME_DELETED:
		case WSAECONNRESET:
			// Ignore these and try again
			break;
		case ERROR_IO_PENDING:
			tryToAccepNewConn = false;
			break;
		default:
			// These happen if use socket upper to MaxUserPort. or socket still be bounded.
			throw Exception(lastError);
			break;
		}
	}
	return false;
}

bool Connection::sendFileAsync(HANDLE fileHandle, uint64_t offset, uint32_t numberOfBytesToWrite) {
    pSendBuffer_->reset();
	pSendBuffer_->Offset = static_cast<DWORD>(offset);
	pSendBuffer_->OffsetHigh = static_cast<DWORD>(offset >> 32);
    
	lastOptFlags_ = details::IOFlags::IO_SEND_FILE_PENDDING;

	auto retval = details::WsaExtAPI::getInstance().transmitFile(acceptSocket_,
                                            fileHandle,
											numberOfBytesToWrite,
                                            0,
                                            pSendBuffer_.get(),
                                            nullptr,
											TF_USE_KERNEL_APC);
    if (retval) {
        lastOptFlags_ = details::IOFlags::IO_SEND_FILE_COMPLETED;
        return true;
    }
    
	auto lastError = ::GetLastError();
    if (lastError != ERROR_IO_PENDING) {
        throw Exception(lastError);
    }
    return false;
}

bool Connection::disconnectAsync() {
    pSendBuffer_->reset();
    pReceiveBuffer_->reset();
    
	resetOverlappedValue();

    RAPID_ENSURE(hasUpdataAcceptContext_ == true);

    lastOptFlags_ = details::IOFlags::IO_DISCONNECT_PENDDING;
    
	if (details::WsaExtAPI::getInstance().disconnectEx(acceptSocket_, this, TF_REUSE_SOCKET, 0)) {
        lastOptFlags_ = details::IOFlags::IO_DISCONNECT_COMPLETED;
        return true;
    }
    
	auto lastError = ::GetLastError();
    if (lastError != ERROR_IO_PENDING) {
        throw Exception(lastError);
    }
    return false;
}

bool Connection::receiveAsync(char * __restrict buffer, uint32_t bufferLen, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteRecv) {
    WSABUF wsabuf = { bufferLen, buffer };
    return receiveAsync(&wsabuf, 1, pBuffer, numByteRecv);
}

bool Connection::sendAsync() {
	return pSendBuffer_->send(shared_from_this());
}

details::SocketAddress const & Connection::getRemoteSocketAddress() const noexcept {
	return accpetedAddressInfo_.remoteAddess;
}

details::SocketAddress const & Connection::getLocalSocketAddress() const noexcept {
	return accpetedAddressInfo_.localAddress;
}

void Connection::sendAndDisconnec() {
	auto pBuffer = getSendBuffer();
	if (pBuffer->isEmpty()) {
		acceptSocket_.shutdownSend();
		disconnect();
	}
}

void Connection::onSend(IoBuffer *pBuffer, uint32_t bytesTransferred) {
	RAPID_LOG_TRACE_FUNC();
	
	pBuffer->retrieve(bytesTransferred);

	if (pBuffer->send(shared_from_this())) {
		if (isSendShutdown_) {
			acceptSocket_.shutdownSend();
			disconnect();
		} else {
			pSendBuffer_->onComplete(shared_from_this());
		}
	}
}

void Connection::onReceive(IoBuffer *pBuffer, uint32_t bytesTransferred) {
	RAPID_LOG_TRACE_FUNC();

	if (bytesTransferred > 0) {
		pBuffer->advanceWriteIndex(bytesTransferred);
		pReceiveBuffer_->onComplete(shared_from_this());
	} else {
		RAPID_LOG_TRACE() << "Peer shutdwon send";		
		halfClosedState_ = GRACEFUL_SHUTDOWN;		
		acceptSocket_.shutdownRecv();
		if (pSendBuffer_->isEmpty()) {
			sendAndDisconnec();
		}
	}
}

void Connection::onConnectionError(uint32_t error) {
	RAPID_LOG_INFO() << " Connection error! (" << std::dec << error << ")";

	if (error == WSAESHUTDOWN) {
	} else if (error == WSAECONNRESET || error == WSAECONNABORTED) {		
		onConnectionReset();
	} else {
		throw Exception(error);
	}
}

void Connection::onConnectionReset() {
	RAPID_LOG_TRACE_FUNC() << " Connection reset!";
	
	isSendShutdown_ = true;
	isRecvShutdown_ = true;

	disconnect();
}

bool Connection::receiveAsync(WSABUF * __restrict iovec, uint32_t numIovec, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteRecv) {
	RAPID_LOG_TRACE_FUNC();

    DWORD returnFlag = 0;

	pBuffer->ioFlag = details::IOFlags::IO_RECV_PENDDING;

    auto ret = ::WSARecv(acceptSocket_.socketFd(),
                         iovec,
                         numIovec,
                         reinterpret_cast<LPDWORD>(numByteRecv),
                         &returnFlag,
                         pBuffer,
                         nullptr);

	if (ret == 0 && *numByteRecv > 0) {
		pBuffer->ioFlag = details::IOFlags::IO_RECV_COMPLETED;
        return true;
    }

    auto lastError = ::GetLastError();
    if (lastError != ERROR_IO_PENDING) {
		if (lastError == ERROR_SUCCESS) {
			onReceive(pBuffer, 0);
		} else {
			onConnectionError(lastError);
		}
    }
    return false;
}

bool Connection::sendAsync(WSABUF * __restrict iovec, uint32_t numIovec, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteSend) {
	RAPID_LOG_TRACE_FUNC();

	pBuffer->ioFlag = details::IOFlags::IO_SEND_PENDDING;

    auto ret = ::WSASend(acceptSocket_.socketFd(),
                         iovec,
                         numIovec,
                         reinterpret_cast<LPDWORD>(numByteSend),
                         0,
                         pBuffer,
                         nullptr);

	if (ret == 0 && *numByteSend > 0) {
		pBuffer->ioFlag = details::IOFlags::IO_SEND_COMPLETED;
        return true;
    }

    auto lastError = ::GetLastError();
    if (lastError != ERROR_IO_PENDING) {
		onConnectionError(lastError);
    }
    return false;
}

bool Connection::sendAsync(char const * __restrict buffer, uint32_t bufferLen, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteSend) {
    RAPID_ENSURE(bufferLen > 0);
    WSABUF wsabuf = { bufferLen, const_cast<char*>(buffer) };
    return sendAsync(&wsabuf, 1, pBuffer, numByteSend);
}

void Connection::resetOverlappedValue() noexcept {
    Internal = 0;
    InternalHigh = 0;
    Offset = 0;
    OffsetHigh = 0;
    Pointer = nullptr;
    hEvent = nullptr;
}

void Connection::getAcceptPairAddress(details::SocketAddressPair &pair) const {
    SOCKADDR *pLocalSockAddr = nullptr;
    SOCKADDR *pRemoteSockAddr = nullptr;

	auto localSockAddrLen = 0;
	auto remoteSockAddrLen = 0;

    RAPID_ENSURE(lastOptFlags_ == details::IOFlags::IO_ACCEPT_PENDDING
                 || lastOptFlags_ == details::IOFlags::IO_ACCEPT_COMPLETED);

	details::WsaExtAPI::getInstance().getAcceptAddress(pReceiveBuffer_->begin(),
		acceptSize_.recvLen,
		acceptSize_.localAddrLen,
		acceptSize_.remoteAddrLen,
        &pLocalSockAddr,
        &localSockAddrLen,
        &pRemoteSockAddr,
        &remoteSockAddrLen);

    if (!pLocalSockAddr || !pRemoteSockAddr) {
        throw Exception();
    }

    pair.localAddress.setFromSockAddr(pLocalSockAddr, localSockAddrLen);
    pair.remoteAddess.setFromSockAddr(pRemoteSockAddr, remoteSockAddrLen);
}

uint32_t Connection::getConnectTime() const {
    uint32_t connectTime = 0;
    INT optLen = sizeof(uint32_t);

    auto retval = acceptSocket_.getSockOpt(SOL_SOCKET, SO_CONNECT_TIME, &connectTime, &optLen);
    if (retval < 0) {
        throw Exception();
    }
    return connectTime;
}

void Connection::onAcceptConnection(uint32_t acceptSize) {
	updateAcceptContext();
    
    getAcceptPairAddress(accpetedAddressInfo_);

    if (acceptSize > 0) {
        pReceiveBuffer_->advanceWriteIndex(acceptSize);
    }
	pAcceptBuffer_->onComplete(shared_from_this());
}

void Connection::addReuseTimingWheel() {
	auto pConn = shared_from_this();

	pTimeWaitReuseTimer_->add(platform::TcpIpParameters::getInstance().getTcpTimedWaitDelay(), true, [pConn]() {
		RAPID_LOG_INFO() << "Background reuse socket";
		if (!pConn->hasUpdataAcceptContext_) { // 未Accept連線不重新投遞acceptAsync
			return;
		}
		try {
			pConn->acceptAsync();
		} catch (Exception const &e) {
			RAPID_LOG_FATAL() << e.what();
		}
	});
}

void Connection::onDisconnected() {
	RAPID_LOG_TRACE_FUNC();
	pDisconnectBuffer_->onComplete(shared_from_this());
	isReuseSocket_ = true;
	if (halfClosedState_ == ACTIVE_CLOSE) {
        // 主動關閉連線會讓Tcp連線狀態進入TIME_WAIT, 所以加入accept pending queue中.
		addReuseTimingWheel();
    } else {
		RAPID_LOG_TRACE() << "Graceful shutdown finished!";
        // 不經過TIME-WAIT等待直接投遞Accept.
        acceptAsync();
    }
}

void Connection::disconnect() {
	RAPID_LOG_TRACE_FUNC();
    if (disconnectAsync()) {
        onDisconnected();
    }
}

void Connection::cancelPendingRequest() {
	details::WsaExtAPI::getInstance().cancelPendingIoRequest(acceptSocket_, this);
	details::WsaExtAPI::getInstance().cancelPendingIoRequest(acceptSocket_, pSendBuffer_.get());
	details::WsaExtAPI::getInstance().cancelPendingIoRequest(acceptSocket_, pReceiveBuffer_.get());
	details::WsaExtAPI::getInstance().cancelPendingIoRequest(acceptSocket_, pPostBuffer_.get());
}

void Connection::abortConnection(Exception const &e) {
	RAPID_LOG_WARN() << "Exception: " << std::dec << e.error() << ", " << e.what();

    cancelPendingRequest();
	
	// More socket erorr code:
	// See https://msdn.microsoft.com/en-us/library/aa924071.aspx
	switch (e.error()) {
    case ERROR_NETNAME_DELETED:
        if (getConnectTime() != MAXDWORD) {
            disconnect();
        } else {
			addReuseTimingWheel();
        }
        break;
	case WSAECONNABORTED: // 10053
		disconnect();
		break;
	case ERROR_SUCCESS:
	case WSAECONNRESET: // 10054 (Client跳過四次優雅分手直接關閉連線)
	default:
		break;
    }
}

void Connection::onCompletion(IoBuffer *pBuffer, uint32_t bytesTransferred) {
	if (lastOptFlags_ != details::IOFlags::IO_ACCEPT_PENDDING) {
        RAPID_ENSURE(hasUpdataAcceptContext_ == true);
    }

	auto opt = lastOptFlags_;
	if (opt != details::IOFlags::IO_DISCONNECT_PENDDING && pBuffer != nullptr) {
		opt = pBuffer->ioFlag;
	}

    switch (opt.flags) {
    case details::IOFlags::IO_ACCEPT_PENDDING:
		onAcceptConnection(bytesTransferred);
        break;
    case details::IOFlags::IO_DISCONNECT_PENDDING:
        onDisconnected();
        break;
    case details::IOFlags::IO_SEND_PENDDING:
        onSend(pBuffer, bytesTransferred);
        break;
    case details::IOFlags::IO_RECV_PENDDING:
        onReceive(pBuffer, bytesTransferred);
        break;
    case details::IOFlags::IO_SEND_FILE_PENDDING:
		onSend(pBuffer, bytesTransferred);
        break;
	case details::IOFlags::IO_POST_PENDDING:
		pPostBuffer_->onComplete(shared_from_this());
		break;
    default:
		// NOTE: Not accept connection, peer abort connection!
        RAPID_ENSURE("Unknown IO operation flag" && 0);
        break;
    }
}

void Connection::onIoCompletion(IoBuffer *pBuffer, uint32_t bytesTransferred) {
    try {
		onCompletion(pBuffer, bytesTransferred);
    } catch (Exception const &e) {
		RAPID_LOG_FATAL() << e.what();
		halfClosedState_ = ACTIVE_CLOSE;
        abortConnection(e);
    } catch (ExceptionWithMinidump const &e) {
        RAPID_LOG_FATAL() << e.what();
		halfClosedState_ = ACTIVE_CLOSE;
		disconnectAsync();
	} catch (std::exception const &e) {
		RAPID_LOG_FATAL() << e.what();
		halfClosedState_ = ACTIVE_CLOSE;
		disconnectAsync();
	}
}

}
