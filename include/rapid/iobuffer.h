//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <atomic>

#include <rapid/platform/platform.h>
#include <rapid/details/ioflags.h>
#include <rapid/details/buffer.h>
#include <rapid/details/socket.h>

namespace rapid {

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;

namespace details {
class Socket;
}

class IoBuffer : public OVERLAPPED {
public:
	IoBuffer();

    IoBuffer(uint32_t prependSize, details::BlockFactory &factory);

    IoBuffer(IoBuffer const &) = delete;
    IoBuffer& operator=(IoBuffer const &) = delete;

    ~IoBuffer();

	bool isCompleted() const noexcept;

    uint32_t size() const noexcept;

    bool isEmpty() const;

    bool isFulled() const;

	IoBuffer& append(char const *buffer, uint32_t bufferLen);

	IoBuffer& append(char ch);

	IoBuffer& append(std::string const &str);

    void retrieve(uint32_t bufferLen);

    std::string readAll();

    uint32_t read(char *buffer, uint32_t bufferLen);

    void read(std::ostream &stream);

    void read(std::ostream &stream, uint32_t len);

    std::string read(uint32_t size);

    uint32_t prependableBytes() const noexcept;

    uint32_t readable() const noexcept;

    uint32_t writeable() const noexcept;

	uint32_t getWrittenSize() const noexcept;

    void reset() noexcept;

    char * writeData();

    char * peek();

    char const * writeData() const;

    char const * peek() const;

	bool readSome(std::shared_ptr<Connection> pConn);

	bool readSome(std::shared_ptr<Connection> pConn, uint32_t requireSize);

    void resetOverlappedValue() noexcept;

    bool send(std::shared_ptr<Connection> pConn);

    void advanceWriteIndex(uint32_t size);

    void advanceReadIndex(uint32_t size);

    char * begin();

    char const * begin() const;

    void makeWriteSpace(uint32_t requireSize);

    void makeWriteableSpace(uint32_t requireSize);

	static uint32_t goodSize() noexcept;

    friend std::ostream& operator<< (std::ostream& ostr, IoBuffer const &buffer);

#ifdef ENABLE_LOCK_MEMORY
	void lockOverlappedRange(details::Socket &socket);
#endif

	template <typename EventHandler>
	void setCompleteHandler(EventHandler &&handler) noexcept;

	void onComplete(ConnectionPtr &pConn) const;

	details::IOFlags ioFlag;
private:
    uint32_t writeIndex_;
    uint32_t readIndex_;
    uint32_t prependable_;
	details::Buffer buffer_;
	std::function<void(ConnectionPtr&)> handler_;
	std::atomic<bool> mutable isCompleted_;
};

__forceinline bool IoBuffer::isCompleted() const noexcept {
	//return HasOverlappedIoCompleted(this);
	return isCompleted_;
}

template <typename EventHandler>
__forceinline void IoBuffer::setCompleteHandler(EventHandler &&handler) noexcept {
	handler_ = std::move(handler);
}

__forceinline void IoBuffer::onComplete(ConnectionPtr &pConn) const {
	isCompleted_ = true;
	handler_(pConn);
}

__forceinline bool IoBuffer::isEmpty() const {
    return readable() == 0;
}

__forceinline bool IoBuffer::isFulled() const {
    return writeable() == 0;
}

}
