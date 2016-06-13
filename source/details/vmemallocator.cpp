//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/logging/logging.h>
#include <rapid/utils/singleton.h>
#include <rapid/utils/scopeguard.h>
#include <rapid/platform/platform.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/privilege.h>
#include <rapid/details/contracts.h>
#include <rapid/details/vmemallocator.h>

namespace rapid {

namespace details {

VMemAllocator::VMemAllocator(uint32_t size, bool useLargePages)
    : MemAllocator(size, useLargePages) {
    pBaseAddress_ = reinterpret_cast<char*>(::VirtualAlloc(nullptr, memoryAllocateSize_, MEM_RESERVE, PAGE_READWRITE));
    if (!pBaseAddress_) {
        throw Exception();
    }
}

VMemAllocator::~VMemAllocator() {
    if (!pBaseAddress_) {
        return;
    }
    if (!::VirtualFree(pBaseAddress_, 0, MEM_RELEASE)) {
		RAPID_LOG_ERROR() << "Release virtual memory failure!";
    }
}

void VMemAllocator::protect(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    DWORD oldProtect = 0;
    if (!::VirtualProtect(addr, size, PAGE_READONLY, &oldProtect)) {
        throw Exception();
    }
}

void VMemAllocator::query(char *addr, MEMORY_BASIC_INFORMATION *info) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    if (!::VirtualQuery(addr, info, sizeof(MEMORY_BASIC_INFORMATION))) {
        throw Exception();
    }
}

char * VMemAllocator::commit(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
	DWORD flags = useLargePages_ ? (MEM_COMMIT | MEM_LARGE_PAGES) : (MEM_COMMIT);
    auto tmp = ::VirtualAlloc(addr,
		size, 
		flags,
		PAGE_READWRITE);
    RAPID_ENSURE(tmp != nullptr);
	if (useLargePages_) {
		if (!::VirtualLock(addr, size)) {
			throw Exception();
		}
	}
    return reinterpret_cast<char *>(tmp);
}

void VMemAllocator::decommit(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    if (!::VirtualFree(addr, size, MEM_DECOMMIT)) {
        throw Exception();
    }
}

}

}
