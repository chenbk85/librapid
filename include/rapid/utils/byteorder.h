//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

namespace rapid {

namespace utils {

enum Endianness : uint32_t {
	LITTLE_ENDIAN = 0x00000001,
	BIG_ENDIAN = 0x01000000
};

template <typename T>
static inline void swapBytes(T *p, size_t len) {
	for (size_t i = 0; i < len / 2; i++) {
		auto tmp = p[len - i - 1];
		p[len - i - 1] = p[i];
		p[i] = tmp;
	}
}

template <typename T>
static inline void swapBytes(T * __restrict dest, T const * __restrict src, size_t len) {
	for (size_t i = 0; i < len; i++) {
		dest[i] = src[len - i - 1];
	}
}

template <typename T>
static inline void swapBytes(T ** __restrict dest, T const * __restrict src, size_t len) {
	auto *p = *dest;
	for (size_t i = 0; i < len; i++) {
		p[i] = src[len - i - 1];
	}
	*dest += len;
}

template <typename T, size_t Size = sizeof(T)>
struct Swapper;

template <typename T>
struct Swapper < T, 1 > final {
	inline static T swap(T value) throw() {
		return value;
	}
};

template <typename T>
struct Swapper < T, 2 > final {
	inline static T swap(T value) throw() {
		return T(((value & 0x00ff) << 8) | ((value & 0xff00) >> 8));
	}
};

template <typename T>
struct Swapper < T, 4 > final {
	inline static T swap(T value) throw() {
		return T(((value & 0x000000ffU) << 24) |
			((value & 0x0000ff00U) << 8) |
			((value & 0x00ff0000U) >> 8) |
			((value & 0xff000000U) >> 24));
	}
};

template <typename T>
struct Swapper < T, 8 > final {
	inline static T swap(T value) throw() {
		return T(((value & 0x00000000000000ffULL) << 56) |
			((value & 0x000000000000ff00ULL) << 40) |
			((value & 0x0000000000ff0000ULL) << 24) |
			((value & 0x00000000ff000000ULL) << 8) |
			((value & 0x000000ff00000000ULL) >> 8) |
			((value & 0x0000ff0000000000ULL) >> 24) |
			((value & 0x00ff000000000000ULL) >> 40) |
			((value & 0xff00000000000000ULL) >> 56));
	}
};

template <Endianness sourceEndian, Endianness destEndian>
class EndianHelper final {
public:
	EndianHelper() = delete;

	EndianHelper(EndianHelper<sourceEndian, destEndian> const &) = delete;
	EndianHelper<sourceEndian, destEndian> &operator=(EndianHelper<sourceEndian, destEndian> const &) = delete;

	static inline int8_t readSint8(char const **buffer) throw() {
		auto value = read<std::int8_t>(*buffer);
		*buffer += sizeof(int8_t);
		return value;
	}

	static inline int16_t readSint16(char const **buffer) throw() {
		auto value = read<int16_t>(*buffer);
		*buffer += sizeof(int16_t);
		return value;
	}

	static inline int32_t readSint32(char const **buffer) throw() {
		auto value = read<int32_t>(*buffer);
		*buffer += sizeof(int32_t);
		return value;
	}

	static inline uint8_t readUint8(char const **buffer) throw() {
		auto value = read<uint8_t>(*buffer);
		*buffer += sizeof(uint8_t);
		return value;
	}

	inline static uint16_t readUint16(char const **buffer) throw() {
		auto value = read<uint16_t>(*buffer);
		*buffer += sizeof(uint16_t);
		return value;
	}

	static inline uint32_t readUint32(char const **buffer) throw() {
		auto value = read<uint32_t>(*buffer);
		*buffer += sizeof(uint32_t);
		return value;
	}

	static inline double readFloat(char const **buffer) throw() {
		auto value = read<float>(*buffer);
		*buffer += sizeof(float);
		return value;
	}

	static inline double readDouble(char const **buffer) throw() {
		auto value = read<double>(*buffer);
		*buffer += sizeof(double);
		return value;
	}

	static inline uint32_t readUint24Big(char const **buffer) throw() {
		uint32_t value = 0;
		swapBytes(reinterpret_cast<char*>(&value), *buffer, 3);
		*buffer += 3;
		return value;
	}

	static inline uint16_t readUint16Le(char const **buffer) throw() {
		return swapToLittleEndian(readUint16(buffer));
	}

	static inline uint32_t readUint32Le(char const **buffer) throw() {
		return swapToLittleEndian(readUint32(buffer));
	}

	static inline uint16_t readUint16Big(char const **buffer) throw() {
		return swapToBigEndian(readUint16(buffer));
	}

	static inline uint32_t readUint32Big(char const **buffer) throw() {
		return swapToBigEndian(readUint32(buffer));
	}

	template <typename T>
	static inline T swapToBigEndian(T value) throw() {
		return maybeSwap<sourceEndian, BIG_ENDIAN>(value);
	}

	template <typename T>
	static inline T swapToLittleEndian(T value) throw() {
		return maybeSwap<sourceEndian, LITTLE_ENDIAN>(value);
	}

	template <typename T>
	static inline void copyAndSwapTo(void* dest, T const * source, size_t count) throw() {
		copyAndSwapTo<sourceEndian, destEndian, T>(dest, source, count);
	}
protected:
	template <typename T>
	static inline T read(void const *p) throw() {
		union {
			T value;
			uint8_t buffer[sizeof(T)];
		} u;

		std::memcpy(u.buffer, p, sizeof(T));
		return u.value;
	}

	template <Endianness S, Endianness D, typename T>
	static inline T maybeSwap(T value) throw() {
		if (S == D) {
			return value;
		}
		return Swapper<T>::swap(value);
	}

	template <Endianness S, Endianness D, typename T>
	static inline void copyAndSwapTo(void* dest, const T* source, size_t count) throw() {
		if (S == D) {
			memcpy(dest, source, count * sizeof(T));
			return;
		}
		auto* byteDestPtr = static_cast<uint8_t*>(dest);
		for (size_t i = 0; i < count; ++i) {
			T value = maybeSwap<S, D>(source[count - i - 1]);
			memcpy(byteDestPtr, &value, sizeof(T));
			byteDestPtr += sizeof(T);
		}
	}
};

struct IsLittleEndian final {
	static const bool value = ((0xFFFFFFFF & 1) == LITTLE_ENDIAN);
};

struct IsBigEndian final {
	static const bool value = ((0xFFFFFFFF & 1) == BIG_ENDIAN);
};

struct SystemEndianHelper final {
	static const auto value =
		IsLittleEndian::value ? LITTLE_ENDIAN : BIG_ENDIAN;
	static const auto opposite =
		value == LITTLE_ENDIAN ? BIG_ENDIAN : LITTLE_ENDIAN;
};

using LittleEndian = EndianHelper<LITTLE_ENDIAN, BIG_ENDIAN>;
using BigEndian = EndianHelper<BIG_ENDIAN, LITTLE_ENDIAN>;
using NoEndian = EndianHelper<LITTLE_ENDIAN, LITTLE_ENDIAN>;

using SystemEndian = EndianHelper<SystemEndianHelper::value, SystemEndianHelper::opposite>;

}

}