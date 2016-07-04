//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/utils/singleton.h>
#include <rapid/utils/scopeguard.h>
#include <rapid/utils/stringutilis.h>

#include <rapid/details/common.h>
#include <rapid/details/contracts.h>
#include <rapid/details/numavmemallocator.h>
#include <rapid/details/vmemallocator.h>

#include <rapid/platform/utils.h>
#include <rapid/platform/privilege.h>

#include <rapid/logging/logging.h>

#include <rapid/details/blockfactory.h>

namespace rapid {

namespace details {

static void getWorkingSetSize(HANDLE process, SIZE_T &minimumWorkingSetSiz, SIZE_T &maximumWorkingSetSize) {
    if (!::GetProcessWorkingSetSize(process, &minimumWorkingSetSiz, &maximumWorkingSetSize)) {
        throw Exception();
    }
}

static void setWorkingSetSize(HANDLE process, SIZE_T minimumWorkingSetSiz, SIZE_T maximumWorkingSetSize) {
    if (!::SetProcessWorkingSetSize(process, minimumWorkingSetSiz, maximumWorkingSetSize)) {
        throw Exception();
    }
}

static void expandWorkingSetSize(MemAllocatorPtr pAllocator) {
	// Open current process object.
	auto processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, ::GetCurrentProcessId());

	std::unique_ptr<void, decltype(&::CloseHandle)> process(processHandle, &::CloseHandle);
	auto const padPageSize = platform::SystemInfo::getInstance().getPageSize() * 20;

	SIZE_T minimumWorkingSetSiz = 0;
    SIZE_T maximumWorkingSetSize = 0;
    
	// Get current prcoess workingset size.
	getWorkingSetSize(process.get(), minimumWorkingSetSiz, maximumWorkingSetSize);
    if (pAllocator->size() > minimumWorkingSetSiz) {
        minimumWorkingSetSiz = pAllocator->size() + padPageSize;
        maximumWorkingSetSize = pAllocator->size() + maximumWorkingSetSize + padPageSize;
		setWorkingSetSize(process.get(), minimumWorkingSetSiz, maximumWorkingSetSize);
    }
}

static void enableLockMemory() {
	// Open current process object.
	auto processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, ::GetCurrentProcessId());
	std::unique_ptr<void, decltype(&::CloseHandle)> process(processHandle, &::CloseHandle);

	TOKEN_PRIVILEGES old = { 0 };
	processHandle = platform::getProcessToken(process.get());
	std::unique_ptr<void, decltype(&::CloseHandle)> token(processHandle, &::CloseHandle);
	platform::enablePrivilege(SE_LOCK_MEMORY_NAME, token.get(), &old);
}

static DWORD getAllocateBufferSize(bool useLargePages, uint32_t maxPageCount, uint32_t bufferSize, uint32_t &granularity) {
	if (useLargePages) {
		enableLockMemory();
	}

	auto largePageMinimum = useLargePages ? ::GetLargePageMinimum() : 0;

	granularity = (largePageMinimum == 0 ? platform::SystemInfo::getInstance().getPageBoundarySize() : largePageMinimum);
	auto desiredSize = bufferSize * maxPageCount;
	auto actualSize = details::roundUp(desiredSize, granularity);

	if (actualSize > MAXDWORD) {
		actualSize = (MAXDWORD / granularity) * granularity;
	}

	// 全部分割完畢後的記憶體page的數量
	auto receiveBuffersAllocated = std::min<DWORD>(maxPageCount, static_cast<DWORD>(actualSize / bufferSize));
	return actualSize;
}

std::shared_ptr<BlockFactory> BlockFactory::createBlockFactory(uint8_t numNumaNode, uint32_t maxPageCount, uint32_t bufferSize) {
	// 配置的時候的記憶體page的大小
	uint32_t maxPageBoundarySize = 0;

	// 全部記憶體的大小
	uint32_t allocPoolSize = 0;

	auto useLargePages = false;

	// 實驗性質: 使用Large Page可以減少CPU進行記憶體轉譯的時間(Page數量變少).
#ifdef ENABLE_LARGE_PAGES
	useLargePages = true;

	try {
		allocPoolSize = getAllocateBufferSize(useLargePages, maxPageCount, bufferSize, maxPageBoundarySize);
		RAPID_LOG_INFO() << "Enable large-pages success!";
	}
	catch (Exception const &e) {
		allocPoolSize = getAllocateBufferSize(false, maxPageCount, bufferSize, maxPageBoundarySize);
		RAPID_LOG_ERROR() << "Enable large-pages failure! " << e.what();
	}

#ifdef ENABLE_LOCK_MEMORY
	// 實驗性質: 可以預先將virtual memory鎖定, 避免io manager呼叫鎖定virtual memory.
	enableLockMemory();
#endif
#endif

	allocPoolSize = getAllocateBufferSize(useLargePages, maxPageCount, bufferSize, maxPageBoundarySize);
    
	MemAllocatorPtr pAllocator;

	if (platform::SystemInfo::getInstance().isNumaSystem()) {
		pAllocator = std::make_shared<details::NumaVMemAllocator>(numNumaNode, allocPoolSize, useLargePages);
    } else {
		pAllocator = std::make_shared<details::VMemAllocator>(allocPoolSize, useLargePages);
    }
	
	if (useLargePages) {
		expandWorkingSetSize(pAllocator);
	}

	// 使用了Large Page對其每一個Buffer依然可以使用PageSize(記憶體分割後的數量不變)
	return std::make_shared<BlockFactory>(pAllocator, maxPageCount, bufferSize);
}

BlockFactory::BlockFactory(MemAllocatorPtr pAllocator, uint32_t maxPageCount, uint32_t maxPageBoundarySize)
	: pAllocator_(pAllocator)
	, pageBoundarySize_(maxPageBoundarySize)
	, pBaseAddress_(pAllocator_->getBaseAddress())
	, totalPageCount_(maxPageCount)
	, count_(0) {
}

Block BlockFactory::getBlock() {
	RAPID_ENSURE(count_ < totalPageCount_);
	Block block;
    block.allocateSize = pageBoundarySize_;
	block.pMem = pAllocator_->commit(pBaseAddress_, platform::SystemInfo::getInstance().getPageSize());
    block.memSize = platform::SystemInfo::getInstance().getPageSize();
    pBaseAddress_ += pageBoundarySize_;
	++count_;
    return block;
}

MemAllocatorPtr BlockFactory::getAllocator() const {
	return pAllocator_->shared_from_this();
}

uint32_t BlockFactory::getSlicePageCount() const noexcept {
    return pageBoundarySize_ / platform::SystemInfo::getInstance().getPageSize();
}

}

}
