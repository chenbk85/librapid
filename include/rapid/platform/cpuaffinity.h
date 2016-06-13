//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <thread>
#include <ostream>
#include <vector>

#include <rapid/platform/platform.h>

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

class CpuGroupAffinity {
public:
	CpuGroupAffinity();

	void setThreadAffinity(std::thread *thread);

	friend std::ostream& operator<< (std::ostream& ostr, CpuGroupAffinity const &affinity);
private:
	struct CpuGroup {
		WORD      active;
		WORD	  reserved[1];
		DWORD_PTR activeMask;
		DWORD	  groupWeight;
		DWORD	  activeThreadWeight;
	};
	void initializeProcessorGroupInfo();
	void getCurrentCpuGroup();
	WORD currentGroup_;
	std::vector<CpuGroup> cpuGroups_;
};

bool isNUMASystem();

ProcessorInformation getProcessorInformation();

std::vector<NumaProcessor> getNumaProcessorInformation();

void setThreadAffinity(std::thread* thread, uint8_t numaNode);

void runThreadOnProcessor(std::thread* thread, uint32_t processor);

}

}
