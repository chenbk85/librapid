//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/memorymappedfile.h>

namespace rapid {

namespace platform {

static inline int64_t getFileSizeByHandle(HANDLE handle) {
	FILE_STANDARD_INFO fileInfo;
	if (!::GetFileInformationByHandleEx(handle, FileStandardInfo, &fileInfo, sizeof(fileInfo))) {
		throw rapid::Exception();
	}
	return fileInfo.EndOfFile.QuadPart;
}

MemoryMappedFile::MemoryMappedFile()
	: data_(nullptr)
	, mapping_(nullptr)
	, fileSize_(0) {
}

MemoryMappedFile::MemoryMappedFile(std::string const &filePath, bool mapAllFileSize)
	: data_(nullptr)
	, mapping_(nullptr)
	, fileSize_(0) {
	open(filePath, mapAllFileSize);
}

MemoryMappedFile::~MemoryMappedFile() {
	close();
}

void MemoryMappedFile::open(std::string const & filePath, bool mapAllFileSize) {
	auto handle = ::CreateFileA(filePath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	if (handle != INVALID_HANDLE_VALUE) {
		fileSize_ = getFileSizeByHandle(handle);
		mapping_ = ::CreateFileMapping(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (mapAllFileSize) {
			data_ = reinterpret_cast<char*>(::MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
		}
		::CloseHandle(handle);
	}
}

void MemoryMappedFile::map(int64_t position, uint32_t length) {
	if (data_ != nullptr) {
		::UnmapViewOfFile(data_);
	}
	LARGE_INTEGER li = { 0 };
	li.QuadPart = position;
	data_ = reinterpret_cast<char*>(::MapViewOfFile(mapping_, FILE_MAP_READ, li.HighPart, li.LowPart, length));
	if (!data_) {
		throw Exception();
	}
}

char const * MemoryMappedFile::data() const noexcept {
	return data_;
}

int64_t MemoryMappedFile::getMappedSize() const {
	MEMORY_BASIC_INFORMATION info = { 0 };
	if (!::VirtualQuery(data_, &info, sizeof(info))) {
		throw Exception();
	}
	return info.RegionSize;
}

int64_t MemoryMappedFile::getFileSize() const {
	return fileSize_;
}

void MemoryMappedFile::close() noexcept {
	if (data_) {
		::UnmapViewOfFile(data_);
		data_ = nullptr;
	}		

	if (mapping_) {
		::CloseHandle(mapping_);
		mapping_ = nullptr;
	}
}

}

}
