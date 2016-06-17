//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <vector>
#include <iterator>

#include <rapid/utils/singleton.h>

#include <rapid/details/contracts.h>
#include <rapid/details/common.h>
#include <rapid/details/blockfactory.h>

#include <rapid/platform/utils.h>

#include <rapid/connection.h>
#include <rapid/iobuffer.h>

namespace rapid {

std::ostream& operator << (std::ostream& ostr, IoBuffer const &buffer) {
	details::dumpHex(ostr, buffer.peek(), buffer.readable());
    return ostr;
}

IoBuffer::IoBuffer()
	: writeIndex_(0)
	, readIndex_(0)
	, prependable_(0) {
	isCompleted_ = false;
}

IoBuffer::IoBuffer(uint32_t prependSize, details::BlockFactory &factory)
	: writeIndex_(prependSize)
	, readIndex_(prependSize)
	, prependable_(prependSize)
	, buffer_(factory.getBlock(), factory.getAllocator()) {
	isCompleted_ = false;
}

IoBuffer::~IoBuffer() {
}

#ifdef ENABLE_LOCK_MEMORY
void IoBuffer::lockOverlappedRange(details::Socket &socket) {
	// This tells the OS that a block of memory will be re-used, 
	// and should be kept locked in memory until the file handle is closed. 
	// Of course if you won't be performing I/O with a handle very often,
	// it might just waste memory.
	if (!::SetFileIoOverlappedRange(socket.handle(), reinterpret_cast<PUCHAR>(begin()), size())) {
		throw Exception();
	}
}
#endif

uint32_t IoBuffer::readable() const noexcept {
    return writeIndex_ - readIndex_;
}

uint32_t IoBuffer::writeable() const noexcept {
    return size() - writeIndex_;
}

uint32_t IoBuffer::getWrittenSize() const noexcept {
	return readable();
}

uint32_t IoBuffer::prependableBytes() const noexcept {
    return readIndex_;
}

char* IoBuffer::begin() {
    return buffer_.begin();
}

char const * IoBuffer::begin() const {
    return buffer_.begin();
}

void IoBuffer::advanceWriteIndex(uint32_t size) {
    writeIndex_ += size;
    RAPID_ENSURE(writeIndex_ <= this->size());
}

void IoBuffer::advanceReadIndex(uint32_t size) {
    readIndex_ += size;
    RAPID_ENSURE(readIndex_ <= writeIndex_);
}

uint32_t IoBuffer::goodSize() noexcept {
    return platform::SystemInfo::getInstance().getPageSize();
}

uint32_t IoBuffer::size() const noexcept {
    return buffer_.size();
}

char * IoBuffer::writeData() {
    RAPID_ENSURE(writeIndex_ <= size());
    return begin() + writeIndex_;
}

char * IoBuffer::peek() {
    RAPID_ENSURE(!isEmpty());
    return begin() + readIndex_;
}

char const * IoBuffer::writeData() const {
    RAPID_ENSURE(writeIndex_ <= size());
    return begin() + writeIndex_;
}

char const * IoBuffer::peek() const {
    RAPID_ENSURE(readIndex_ <= writeIndex_);
    return begin() + readIndex_;
}

bool IoBuffer::send(std::shared_ptr<Connection> pConn) {
	isCompleted_ = false;

	if (isEmpty()) {
		return true;
	}
	uint32_t numByteSend = 0;
	auto pReadData = peek();
	auto readableBytes = readable();
	if (pConn->sendAsync(pReadData, readableBytes, this, &numByteSend)) {
		retrieve(numByteSend);
		return true;
	}
	return false;
}

bool IoBuffer::readSome(std::shared_ptr<Connection> pConn) {
	isCompleted_ = false;

	return readSome(pConn, goodSize());
}

bool IoBuffer::readSome(std::shared_ptr<Connection> pConn, uint32_t requireSize) {
	isCompleted_ = false;

    makeWriteableSpace(requireSize);    
	resetOverlappedValue();    
	uint32_t numBytesRecv = 0;	
	RAPID_ENSURE(requireSize > 0);
    if (pConn->receiveAsync(writeData(), requireSize, this, &numBytesRecv)) {
        advanceWriteIndex(numBytesRecv);
        return true;
    }
    return false;
}

void IoBuffer::makeWriteableSpace(uint32_t requireSize) {
    if (writeable() < requireSize) {
        makeWriteSpace(requireSize);
    }
    RAPID_ENSURE(writeable() >= requireSize);
}

void IoBuffer::makeWriteSpace(uint32_t requireSize) {
    if (writeable() + prependableBytes() < requireSize + prependable_) {
		buffer_.expandSize(writeIndex_ + requireSize);
    } else {
        auto const readableBytes = readable();
        std::copy(begin() + readIndex_,
                  begin() + writeIndex_,
                  stdext::checked_array_iterator<char*>(begin() + prependable_, readableBytes));
        readIndex_ = prependable_;
        writeIndex_ = readIndex_ + readableBytes;
    }
}

IoBuffer& IoBuffer::append(char const *buffer, uint32_t bufferLen) {
	if (writeable() < bufferLen) {
		makeWriteSpace(bufferLen);
	}
	RAPID_ENSURE(writeable() >= bufferLen);
	std::copy(buffer, buffer + bufferLen,
			  stdext::checked_array_iterator<char*>(begin() + writeIndex_, bufferLen));
	advanceWriteIndex(bufferLen);
	return *this;
}

IoBuffer& IoBuffer::append(char ch) {
	return append(&ch, 1);
}

IoBuffer& IoBuffer::append(std::string const &str) {
	return append(str.c_str(), static_cast<uint32_t>(str.length()));
}

void IoBuffer::retrieve(uint32_t bufferLen) {
	RAPID_ENSURE(bufferLen <= readable());
    if (bufferLen < readable()) {
        advanceReadIndex(bufferLen);
	} else {
        reset();
	}
}

uint32_t IoBuffer::read(char *buffer, uint32_t bufferLen) {
    auto const readableBytes = (std::min)(readable(), bufferLen);
    memcpy(buffer, peek(), readableBytes);
    retrieve(readableBytes);
    return readableBytes;
}

void IoBuffer::read(std::ostream &stream) {
    read(stream, readable());
}

void IoBuffer::read(std::ostream &stream, uint32_t len) {
    auto const readableBytes = (std::min)(len, readable());
    stream.write(peek(), readableBytes);
    retrieve(readableBytes);
}

std::string IoBuffer::read(uint32_t size) {
    auto const readableBytes = (std::min)(readable(), size);
    std::string str(peek(), readableBytes);
    retrieve(readableBytes);
    return str;
}

std::string IoBuffer::readAll() {
    RAPID_ENSURE(!isEmpty());
    auto readableBytes = readable();
    auto const startIndex = readIndex_;
    retrieve(readableBytes);
    return std::string(begin() + startIndex, readableBytes);
}

void IoBuffer::reset() throw() {
    writeIndex_ = prependable_;
    readIndex_ = prependable_;
}

void IoBuffer::resetOverlappedValue() throw() {
    Internal = 0;
    InternalHigh = 0;
    Offset = 0;
    OffsetHigh = 0;
    Pointer = nullptr;
    hEvent = nullptr;
}

}
