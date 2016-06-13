//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/details/contracts.h>
#include <rapid/details/numavmemallocator.h>

namespace rapid {

namespace details {

NumaVMemAllocator::NumaVMemAllocator(UCHAR node, uint32_t size, bool useLargePages)
    : MemAllocator(size, useLargePages)
    , numaNode_(node)
    , currentProcess_(::GetCurrentProcess()) {
    pBaseAddress_ = reinterpret_cast<char*>(::VirtualAllocExNuma(currentProcess_,
                                   nullptr,
                                   memoryAllocateSize_,
                                   MEM_RESERVE,
                                   PAGE_READWRITE,
                                   node));
    if (!pBaseAddress_) {
        throw Exception();
    }
}

NumaVMemAllocator::~NumaVMemAllocator() {
}

char * NumaVMemAllocator::commit(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
	DWORD flags = useLargePages_ ? (MEM_COMMIT | MEM_LARGE_PAGES) : (MEM_COMMIT);
    auto tmp = ::VirtualAllocExNuma(currentProcess_, addr, size,
		flags, PAGE_READWRITE, numaNode_);
    if (!tmp) {
        throw Exception();
    }
    return reinterpret_cast<char *>(tmp);
}

void NumaVMemAllocator::decommit(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    if (!::VirtualFreeEx(currentProcess_, addr, size, MEM_DECOMMIT)) {
        throw Exception();
    }
}

void NumaVMemAllocator::protect(char *addr, uint32_t size) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    DWORD oldProtect = 0;
    if (!::VirtualProtectEx(currentProcess_, addr, size, PAGE_READONLY, &oldProtect)) {
        throw Exception();
    }
}

void NumaVMemAllocator::query(char *addr, MEMORY_BASIC_INFORMATION *info) {
    RAPID_ENSURE(addr >= pBaseAddress_ && addr <= pBaseAddress_ + memoryAllocateSize_);
    if (!::VirtualQueryEx(currentProcess_, addr, info, sizeof(MEMORY_BASIC_INFORMATION))) {
        throw Exception(::GetLastError());
    }
}

}

}
