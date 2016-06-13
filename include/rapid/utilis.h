//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <ostream>
#include <algorithm>

#include <rapid/details/contracts.h>

#include <rapid/utils/byteorder.h>
#include <rapid/connection.h>
#include <rapid/iobuffer.h>

namespace rapid {

template <typename Endian = utils::LittleEndian>
inline void writeData(IoBuffer * __restrict pBuffer, uint8_t const * __restrict data, uint32_t dataSize) {
	if (dataSize > pBuffer->writeable()) {
		pBuffer->makeWriteableSpace(dataSize);
	}
	Endian::copyAndSwapTo(pBuffer->writeData(), data, dataSize);
	pBuffer->advanceWriteIndex(dataSize);
}

template <uint32_t N, typename Endian = utils::LittleEndian>
inline void writeData(IoBuffer * __restrict pBuffer, char(&arrayTypeBuffer)[N]) {
	writeData<Endian>(pBuffer, arrayTypeBuffer, N);
}

template <typename Endian = utils::LittleEndian, typename T>
inline void writeData(IoBuffer * pBuffer, T value) {
	writeData<Endian>(pBuffer, reinterpret_cast<uint8_t *>(&value), sizeof(T));
}

template <typename Endian = utils::LittleEndian>
inline void writeUint24(IoBuffer * pBuffer, uint32_t value) {
	writeData<Endian>(pBuffer, reinterpret_cast<uint8_t *>(&value), 3);
}

static inline uint8_t readUint8(IoBuffer * pBuffer) {
	RAPID_ENSURE(pBuffer->readable() >= 1);
	char const *data = pBuffer->peek();
	pBuffer->retrieve(1);
	return uint8_t(*data);
}

static inline uint32_t readUint24Big(IoBuffer * pBuffer) {
	RAPID_ENSURE(pBuffer->readable() >= 3);
	char const *data = pBuffer->peek();
	auto value = utils::SystemEndian::readUint24Big(&data);
	pBuffer->retrieve(3);
	return value;
}

static inline uint16_t readUint16Big(IoBuffer * pBuffer) {
	RAPID_ENSURE(pBuffer->readable() >= sizeof(uint16_t));
	char const *data = pBuffer->peek();
	auto value = utils::SystemEndian::readUint16Big(&data);
	pBuffer->retrieve(sizeof(uint16_t));
	return value;
}

static inline uint32_t readUint32Big(IoBuffer * pBuffer) {
	RAPID_ENSURE(pBuffer->readable() >= sizeof(uint32_t));
	char const *data = pBuffer->peek();
	auto value = utils::SystemEndian::readUint32Big(&data);
	pBuffer->retrieve(sizeof(uint32_t));
	return value;
}

static inline void format(IoBuffer * __restrict pBuffer, char const * __restrict format, ...) {
	va_list args;
	va_start(args, format);
	auto const retval = ::_vscprintf(format, args);
	pBuffer->makeWriteableSpace(retval + 1); // Pad null string.
	auto writeable = pBuffer->writeable();
	pBuffer->writeData()[0] = '\0';
	auto const writtenSize = ::vsnprintf_s(pBuffer->writeData(), writeable, _TRUNCATE, format, args);
	RAPID_ENSURE(writtenSize != -1);
	pBuffer->advanceWriteIndex(writtenSize);
	va_end(args);
}

static inline bool readUntil(ConnectionPtr pConn, std::ostream &stream, uint32_t totalSize) {
	auto *pBuffer = pConn->getReceiveBuffer();
	uint32_t readableBytes = 0;
	uint32_t writtenSize = 0;

	do {
		writtenSize = static_cast<uint32_t>(stream.tellp());
		if (!pBuffer->isEmpty()) {
			pBuffer->read(stream, totalSize - writtenSize);
			RAPID_ENSURE(writtenSize <= totalSize);
			if (writtenSize == totalSize) {
				return true;
			}
		}

		RAPID_ENSURE(writtenSize < totalSize);
		readableBytes = totalSize - writtenSize;
		readableBytes = (std::min)(static_cast<uint32_t>(pBuffer->goodSize()), readableBytes);
	} while (pBuffer->readSome(pConn->shared_from_this(), readableBytes));
	return false;
}

static inline bool readUntil(ConnectionPtr pConn, uint32_t &writtenSize, uint32_t totalSize,
	std::function<void(char const *, size_t)> parseCallback) {
	auto *pBuffer = pConn->getReceiveBuffer();
	uint32_t readableBytes = 0;

	do {
		if (!pBuffer->isEmpty()) {
			char const *buf = pBuffer->peek();
			readableBytes = pBuffer->readable();
			parseCallback(buf, readableBytes);
			pBuffer->retrieve(readableBytes);
			writtenSize += readableBytes;
			if (writtenSize == totalSize) {
				return true;
			}
		}

		RAPID_ENSURE(writtenSize < totalSize);
		readableBytes = totalSize - writtenSize;
		readableBytes = (std::min)(static_cast<uint32_t>(pBuffer->goodSize()), readableBytes);
	} while (pBuffer->readSome(pConn->shared_from_this(), readableBytes));
	return false;
}

}
