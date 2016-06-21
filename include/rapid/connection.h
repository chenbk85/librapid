//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <rapid/platform/platform.h>

#include <rapid/eventhandler.h>

#include <rapid/exception.h>
#include <rapid/details/socket.h>
#include <rapid/details/socketaddress.h>
#include <rapid/details/ioflags.h>

namespace rapid {

static int64_t constexpr SEND_FILE_MAX_SIZE = INT_MAX - 1;

namespace details {
class IoEventQueue;
class IoEventDispatcher;
class BlockFactory;

class TimingWheel;
using TimingWheelPtr = std::shared_ptr<TimingWheel>;
}

class IoBuffer;

class Connection : public OVERLAPPED, public std::enable_shared_from_this<Connection> {
public:
	Connection(details::TcpServerSocket *listenSocket,
		details::BlockFactory &factory,
		details::TimingWheelPtr reuseTimingWheel);

    virtual ~Connection();

    Connection(Connection const &) = delete;
    Connection& operator=(Connection const &) = delete;

    bool acceptAsync();

	void sendAndDisconnec();

    uint32_t getConnectTime() const;

    void cancelPendingRequest();

	void postAsync(std::function<void(ConnectionPtr)> handler);

    template <typename Lambda>
	void setAcceptEventHandler(Lambda &&handler) {
		pAcceptBuffer_->setCompleteHandler(std::move(handler));
    }

	template <typename Lambda>
	void setReceiveEventHandler(Lambda &&handler) {
		pReceiveBuffer_->setCompleteHandler(std::move(handler));
    }

	template <typename Lambda>
	void setSendEventHandler(Lambda &&handler) {
		pSendBuffer_->setCompleteHandler(std::move(handler));
    }

	template <typename Lambda>
	void setDisconnectEventHandler(Lambda &&handler) {
		pDisconnectBuffer_->setCompleteHandler(std::move(handler));
    }

	void onIoCompletion(IoBuffer *pBuffer, uint32_t bytesTransferred);

    IoBuffer* getReceiveBuffer() const noexcept;

	IoBuffer* getSendBuffer() const noexcept;

	bool sendAsync();

	details::SocketAddress const & getRemoteSocketAddress() const noexcept;

	details::SocketAddress const & getLocalSocketAddress() const noexcept;

private:
	friend class IoBuffer;

	struct AcceptBufferSize {
		static uint32_t constexpr localAddrLen = sizeof(SOCKADDR_STORAGE);
		static uint32_t constexpr remoteAddrLen = sizeof(SOCKADDR_STORAGE);
		static uint32_t constexpr IPV6_ACCEPT_SOCKADDR_SIZE = sizeof(SOCKADDR_STORAGE) * 2 + 64;

		AcceptBufferSize() noexcept;
		explicit AcceptBufferSize(uint32_t acceptBufferSize) noexcept;

		uint32_t bufferSize;
		uint32_t recvLen;
	};

	bool receiveAsync(char * __restrict buffer, uint32_t bufferLen, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteRecv);

	bool receiveAsync(WSABUF * __restrict iovec, uint32_t numIovec, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteRecv);

	bool sendFileAsync(HANDLE fileHandle, uint64_t offset = 0, uint32_t numberOfBytesToWrite = 0);

	bool sendAsync(char const * __restrict buffer, uint32_t bufferLen, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteSend);

	bool sendAsync(WSABUF * __restrict iovec, uint32_t numIovec, IoBuffer * __restrict pBuffer, uint32_t * __restrict numByteSend);

	void onCompletion(IoBuffer *pBuffer, uint32_t bytesTransferred);

    bool disconnectAsync();

    void disconnect();

    void abortConnection(Exception const &e);

    void onSend(IoBuffer *pBuffer, uint32_t bytesTransferred);

	void onReceive(IoBuffer *pBuffer, uint32_t bytesTransferred);

	void onAcceptConnection(uint32_t acceptSize);

	void addReuseTimingWheel();

    void getAcceptPairAddress(details::SocketAddressPair &pair) const;

    void onDisconnected();

	void resetOverlappedValue() noexcept;

	void onConnectionError(uint32_t error);

	void onConnectionReset();

	void updateAcceptContext();

	enum HalfClosedState {
		ACTIVE_CLOSE,
		GRACEFUL_SHUTDOWN,
	};

	bool isReuseSocket_ : 1;
	bool hasUpdataAcceptContext_ : 1;
	bool isSendShutdown_ : 1;
	bool isRecvShutdown_ : 1;
	details::IOFlags lastOptFlags_;
	HalfClosedState halfClosedState_;
	details::TcpServerSocket* pListenSocket_;
	details::TcpSocket acceptSocket_;
	details::TimingWheelPtr pTimeWaitReuseTimer_;
	std::unique_ptr<IoBuffer> pAcceptBuffer_;
	std::unique_ptr<IoBuffer> pSendBuffer_;
	std::unique_ptr<IoBuffer> pReceiveBuffer_;
	std::unique_ptr<IoBuffer> pPostBuffer_;
	std::unique_ptr<IoBuffer> pDisconnectBuffer_;
	AcceptBufferSize acceptSize_;
	details::SocketAddressPair accpetedAddress_;
};

__forceinline IoBuffer* Connection::getReceiveBuffer() const noexcept {
    return pReceiveBuffer_.get();
}

__forceinline IoBuffer* Connection::getSendBuffer() const noexcept {
	return pSendBuffer_.get();
}

}

