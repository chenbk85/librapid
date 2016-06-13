//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <cstdint>
#include <vector>
#include <fstream>

#include <rapid/platform/platform.h>

#include <tlhelp32.h>
#include <Objbase.h>
#include <Winsock2.h>
#include <Psapi.h>

#include <rapid/exception.h>

#include <rapid/details/contracts.h>
#include <rapid/details/common.h>
#include <rapid/platform/utils.h>

namespace rapid {

namespace platform {

struct SocketInitializer {
    SocketInitializer() noexcept {
		auto retval = ::WSAStartup(MAKEWORD(2, 2), &wsadata_);
        if (retval != ERROR_SUCCESS) {
			lastError_ = ::WSAGetLastError();
        } else {
			lastError_ = ERROR_SUCCESS;
        }
    }

    ~SocketInitializer() noexcept {
		if (lastError_ == ERROR_SUCCESS) {
            ::WSACleanup();
        }
    }

    WSADATA	wsadata_;
    uint32_t lastError_;
};

class SystemInfo::SystemInfoImpl {
public:
    SystemInfoImpl();

    ~SystemInfoImpl() noexcept;

    uint32_t getNumberOfProcessors(uint32_t perCPU) const noexcept;

    uint32_t getPageSize() const noexcept;

    size_t getPageBoundarySize() const noexcept;

	uint32_t roundUpToPageSize(uint32_t size) const noexcept;

private:
    SYSTEM_INFO info;
};

SystemInfo::SystemInfoImpl::SystemInfoImpl() {
    ::GetSystemInfo(&info);
}

SystemInfo::SystemInfoImpl::~SystemInfoImpl() noexcept {
}

uint32_t SystemInfo::SystemInfoImpl::getNumberOfProcessors(uint32_t perCPU) const noexcept {
    return perCPU * info.dwNumberOfProcessors;
}

uint32_t SystemInfo::SystemInfoImpl::getPageSize() const noexcept {
    return info.dwPageSize;
}

size_t SystemInfo::SystemInfoImpl::getPageBoundarySize() const noexcept {
    return info.dwAllocationGranularity;
}

uint32_t SystemInfo::SystemInfoImpl::roundUpToPageSize(uint32_t size) const noexcept {
    return details::roundUp(size, getPageSize());
}

SystemInfo::SystemInfo()
    : pInfo_(std::make_unique<SystemInfoImpl>()) {
}

SystemInfo::~SystemInfo() noexcept {
}

uint32_t SystemInfo::getPageSize() const noexcept {
    return pInfo_->getPageSize();
}

uint32_t SystemInfo::getPageBoundarySize() const noexcept {
    return pInfo_->getPageBoundarySize();
}

uint32_t SystemInfo::roundUpToPageSize(uint32_t size) const noexcept {
    return pInfo_->roundUpToPageSize(size);
}

uint32_t SystemInfo::getNumberOfProcessors(uint32_t perCPU) const noexcept {
    return pInfo_->getNumberOfProcessors(perCPU);
}

bool startupWinSocket() {
    static const SocketInitializer socketIniter;
    return socketIniter.lastError_ == ERROR_SUCCESS;
}

std::wstring getApplicationFilePath() {
    wchar_t fileName[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, fileName, MAX_PATH - 1);
    return fileName;
}

std::wstring getApplicationFileName() {
	wchar_t drive[MAX_PATH];
	wchar_t dir[MAX_PATH];
	wchar_t filename[MAX_PATH];
	wchar_t ext[MAX_PATH];
	wchar_t exe_filename[MAX_PATH];

	auto filePath = getApplicationFilePath();
	::_wsplitpath_s(filePath.c_str(),
		drive,
		MAX_PATH,
		dir,
		MAX_PATH,
		filename,
		MAX_PATH,
		ext,
		MAX_PATH);
	::_wmakepath_s(exe_filename, MAX_PATH, nullptr, nullptr, filename, ext);
	return filename;
}

std::string getFinalPathNameByHandle(HANDLE fileHandle) {
    RAPID_ENSURE(fileHandle != INVALID_HANDLE_VALUE);
    char path[MAX_PATH] = { 0 };
    ::GetFinalPathNameByHandleA(fileHandle, path, MAX_PATH - 1, VOLUME_NAME_NT);
    return path;
}

std::vector<uint32_t> enumThreadIds() {
	auto const processId = ::GetCurrentProcessId();
	std::unique_ptr<void, decltype(&::CloseHandle)> snapshotHandle(
		::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0), &::CloseHandle);
	THREADENTRY32 threadEntry = { 0 };
	threadEntry.dwSize = sizeof(THREADENTRY32);
	if (!::Thread32First(snapshotHandle.get(), &threadEntry)) {
		throw Exception();
	}
	std::vector<uint32_t> threadIds;
	for (; ::Thread32Next(snapshotHandle.get(), &threadEntry);) {
		if (threadEntry.th32OwnerProcessID == processId) {
			threadIds.push_back(threadEntry.th32ThreadID);
		}
	}
	return threadIds;
}

void setProcessPriorityBoost(bool enableBoost) {
    if (!::SetProcessPriorityBoost(::GetCurrentProcess(), enableBoost)) {
        throw Exception();
    }
}

int64_t getFileSize(std::string const &filePath) {
	struct __stat64 fileStat;
	_stat64(filePath.c_str(), &fileStat);
	return fileStat.st_size;
}

void prefetchVirtualMemory(char const * virtualAddress, size_t size) {
	// TODO: Windows 2008 not support!
	WIN32_MEMORY_RANGE_ENTRY entry;
	entry.VirtualAddress = const_cast<void*>(reinterpret_cast<const void*>(virtualAddress));
	entry.NumberOfBytes = size;

	if (!::PrefetchVirtualMemory(::GetCurrentProcess(), 1, &entry, 0)) {
		throw rapid::Exception();
	}
}

}

}
