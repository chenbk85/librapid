//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <functional>

#include <rapid/platform/platform.h>
#include <rapid/details/memallocator.h>

namespace rapid {

namespace details {

struct Block {
    Block()
        : pMem(nullptr)
		, allocateSize(0)
		, memSize(0) {
    }
	char *pMem;
    uint32_t allocateSize;
	uint32_t memSize;
};

class BlockFactory {
public:
    using TraverseCallback = std::function<void(MEMORY_BASIC_INFORMATION const &)>;

    static std::shared_ptr<BlockFactory> createBlockFactory(uint8_t numNumaNode, uint32_t maxPageCount, uint32_t bufferSize);

	BlockFactory(MemAllocatorPtr allocator, uint32_t maxPageCount, uint32_t maxPageBoundarySize);

	BlockFactory(BlockFactory const &) = delete;
	BlockFactory& operator=(BlockFactory const &) = delete;

	Block getBlock();

    MemAllocatorPtr getAllocator() const;

	uint32_t getSlicePageCount() const noexcept;

private:
	MemAllocatorPtr pAllocator_;
    char *pBaseAddress_;
	uint32_t pageBoundarySize_;
	uint32_t totalPageCount_;
	uint32_t count_;
};

}

}
