//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include <rapid/utils/singleton.h>

namespace rapid {

namespace platform {

struct ProcessorInformation {
	ProcessorInformation()
		: numaNodeCount(0)
		, processorCoreCount(0)
		, logicalProcessorCount(0)
		, processorL1CacheCount(0)
		, processorL2CacheCount(0)
		, processorL3CacheCount(0)
		, processorPackageCount(0) {
	}

	friend std::ostream& operator<< (std::ostream& ostr, ProcessorInformation const &info);

	uint32_t numaNodeCount;
	uint32_t processorCoreCount;
	uint32_t logicalProcessorCount;
	uint32_t processorL1CacheCount;
	uint32_t processorL2CacheCount;
	uint32_t processorL3CacheCount;
	uint32_t processorPackageCount;
};

struct NumaProcessor {
	NumaProcessor()
		: processor(0)
		, node(0)
		, processorMask(0) {
	}

	friend std::ostream& operator<< (std::ostream& ostr, std::vector<NumaProcessor> const &numaProcessors);

	uint8_t processor;
	uint8_t node;
	uint64_t processorMask;
};

class SystemInfo : public utils::Singleton<SystemInfo> {
public:
    SystemInfo();

    ~SystemInfo() noexcept;

    uint32_t getNumberOfProcessors(uint32_t perCPU) const noexcept;

    uint32_t getPageSize() const noexcept;

	uint32_t getPageBoundarySize() const noexcept;

	uint32_t roundUpToPageSize(uint32_t size) const noexcept;

    bool isNumaSystem();

    uint64_t getNumaNodeProcessorMask(uint8_t node);

    std::vector<NumaProcessor> getNumaProcessorInformation();

    ProcessorInformation getProcessorInformation();

private:
    class SystemInfoImpl;
    std::unique_ptr<SystemInfoImpl> pImpl_;
};

bool startupWinSocket();

std::wstring getApplicationFilePath();

std::wstring getApplicationFileName();

void setProcessPriorityBoost(bool enableBoost);

std::vector<uint32_t> enumThreadIds();

int64_t getFileSize(std::string const &filePath);

void prefetchVirtualMemory(char const * virtualAddress, size_t size);

std::vector<std::string> getInterfaceNameList();

}

}

