//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/platform/utils.h>
#include <rapid/utils/singleton.h>
#include <rapid/details/contracts.h>
#include <rapid/details/buffer.h>

namespace rapid {

namespace details {

Buffer::Buffer() {
}

Buffer::Buffer(Block const &block, MemAllocatorPtr allocator)
    : block_(block)
    , pAllocator_(allocator) {
	commitSize_ = block_.memSize;
}

Buffer::~Buffer() {

}

char* Buffer::begin() const noexcept {
	return block_.pMem;
}

uint32_t Buffer::size() const noexcept {
	return commitSize_;
}

void Buffer::expandSize(uint32_t size) {
	RAPID_ENSURE(size <= block_.allocateSize);
	if (commitSize_ < size) {
        auto roundPageSize = platform::SystemInfo::getInstance().roundUpToPageSize(size);
		pAllocator_->commit(block_.pMem + commitSize_, roundPageSize - commitSize_);
		commitSize_ = roundPageSize;
    }
}

void Buffer::protect() const {
	pAllocator_->protect(block_.pMem, commitSize_);
}

void Buffer::release() const {
	pAllocator_->decommit(block_.pMem, commitSize_);
}

}

}
