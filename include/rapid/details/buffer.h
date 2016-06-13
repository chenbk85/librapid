//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <cstdint>

#include <rapid/details/blockfactory.h>
#include <rapid/details/vmemallocator.h>

namespace rapid {

namespace details {

class Buffer {
public:
	Buffer() = default;

    Buffer(Block const &block, MemAllocatorPtr allocator);

    ~Buffer();

    Buffer(Buffer const &) = delete;
    Buffer& operator=(Buffer const &) = delete;

    void expandSize(uint32_t size);

	uint32_t size() const noexcept;

    void protect() const;

    void release() const;

    char* begin() const noexcept;

private:
    Block block_;
    MemAllocatorPtr pAllocator_;
	uint32_t commitSize_;
};

}

}