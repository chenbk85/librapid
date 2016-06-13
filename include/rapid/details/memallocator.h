//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <rapid/platform/platform.h>

namespace rapid {

namespace details {

class MemAllocator;
using MemAllocatorPtr = std::shared_ptr<MemAllocator>;

class MemAllocator : public std::enable_shared_from_this<MemAllocator> {
public:
    MemAllocator(MemAllocator const &) = delete;
    MemAllocator& operator=(MemAllocator const &) = delete;

    virtual ~MemAllocator() = default;

    virtual void query(char *addr, MEMORY_BASIC_INFORMATION *info) = 0;

    virtual char * commit(char *addr, uint32_t size) = 0;

    virtual void protect(char *addr, uint32_t size) = 0;

    virtual void decommit(char *addr, uint32_t size) = 0;

    char * getBaseAddress() const noexcept {
        return pBaseAddress_;
    }

	uint32_t size() const noexcept {
        return memoryAllocateSize_;
    }

protected:
    MemAllocator(uint32_t size, bool useLargePages)
        : pBaseAddress_(nullptr)
        , memoryAllocateSize_(size)
		, useLargePages_(useLargePages) {
    }

    char *pBaseAddress_;
    uint32_t memoryAllocateSize_;
	bool useLargePages_;
};

}

}
