//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <bitset>

#include <rapid/exception.h>
#include <rapid/platform/platform.h>
#include <rapid/platform/cpuaffinity.h>

namespace rapid {

namespace platform {

static inline uint32_t countSetBits(ULONG_PTR bitMask) {
	uint32_t LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	uint32_t bitSetCount = 0;
	auto bitTest = static_cast<ULONG_PTR>(1) << LSHIFT;
	uint32_t i;

	for (i = 0; i <= LSHIFT; ++i) {
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}
	return bitSetCount;
}

std::ostream& operator << (std::ostream& ostr, ProcessorInformation const &info) {
	ostr << "Number of NUMA nodes: " << info.numaNodeCount << "\r\n"
		<< "Number of physical processor packages: " << info.processorPackageCount << "\r\n"
		<< "Number of processor cores: " << info.processorCoreCount << "\r\n"
		<< "Number of logical processors: " << info.logicalProcessorCount << "\r\n"
		<< "L1/L2/L3 caches: "
		<< info.processorL1CacheCount << " MB"
		<< "/" << info.processorL2CacheCount << " MB"
		<< "/" << info.processorL3CacheCount << " MB";
	return ostr;
}

std::ostream & operator<<(std::ostream & ostr, std::vector<NumaProcessor> const & numaProcessors) {
	for (auto processor : numaProcessors) {
		ostr << "processor: " << (int) processor.processor << "\r\n"
			<< "node: " << (int) processor.node << "\r\n"
			<< "processorMask: " << std::bitset<64>(processor.processorMask).to_string() << "\r\n";
	}
	return ostr;
}

std::ostream & operator<<(std::ostream & ostr, CpuGroupAffinity const & affinity) {
	for (auto i = 0; i < affinity.cpuGroups_.size(); ++i) {
		ostr << "active: " << affinity.cpuGroups_[i].active << "\r\n";
		ostr << "activeMask: " << affinity.cpuGroups_[i].activeMask << "\r\n";
		ostr << "activeThreadWeight: " << affinity.cpuGroups_[i].activeThreadWeight << "\r\n";
		ostr << "groupWeight: " << affinity.cpuGroups_[i].groupWeight;
	}
	return ostr;
}

ProcessorInformation getProcessorInformation() {
	ProcessorInformation info;

	DWORD returnLength = 0;
	uint32_t byteOffset = 0;
	PCACHE_DESCRIPTOR cache = nullptr;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pLogicalProcInfo = nullptr;

	std::vector<uint8_t> buffer;

	if (!::GetLogicalProcessorInformation(nullptr, &returnLength)) {
		auto lastError = ::GetLastError();

		if (lastError == ERROR_INSUFFICIENT_BUFFER) {
			buffer.resize(returnLength);
			pLogicalProcInfo = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.data());
			::GetLogicalProcessorInformation(pLogicalProcInfo, &returnLength);
		} else {
			throw Exception(lastError);
		}
	}

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
		switch (pLogicalProcInfo->Relationship) {
		case RelationNumaNode:
			info.numaNodeCount++;
			break;
		case RelationProcessorCore:
			info.processorCoreCount++;
			// A hyper-threaded core supplies more than one logical processor.
			info.logicalProcessorCount += countSetBits(pLogicalProcInfo->ProcessorMask);
			break;
		case RelationCache:
			cache = &pLogicalProcInfo->Cache;
			if (cache->Level == 1) {
				info.processorL1CacheCount++;
			} else if (cache->Level == 2) {
				info.processorL2CacheCount++;
			} else if (cache->Level == 3) {
				info.processorL3CacheCount++;
			}
			break;
		case RelationProcessorPackage:
			// Logical processors share a physical package.
			info.processorPackageCount++;
			break;
		case RelationGroup:
			break;
		case RelationAll:
			break;
		default:
			break;
		}

		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		pLogicalProcInfo++;
	}
	return info;
}

bool isNUMASystem() {
	ULONG highestNodeNumber = 0;
	if (!::GetNumaHighestNodeNumber(&highestNodeNumber)) {
		throw Exception();
	}
	return highestNodeNumber > 0;
}

uint64_t getNumaNodeProcessorMask(UCHAR node) {
	uint64_t processorMask = 0;
	if (!::GetNumaNodeProcessorMask(node, &processorMask)) {
		throw Exception();
	}
	return processorMask;
}

std::vector<NumaProcessor> getNumaProcessorInformation() {
	std::vector<NumaProcessor> numaProcessorInfo;

	auto processorInfo = getProcessorInformation();
	for (uint32_t i = 0; i < processorInfo.logicalProcessorCount; ++i) {
		NumaProcessor numaProcessor;
		
		numaProcessor.processor = i;
		if (!::GetNumaProcessorNode(i, &numaProcessor.node)) {
			throw Exception();
		}

		if (!::GetNumaNodeProcessorMask(numaProcessor.node, &numaProcessor.processorMask)) {
			throw Exception();
		}
		numaProcessorInfo.push_back(numaProcessor);
	}
	return numaProcessorInfo;
}

void setThreadAffinity(std::thread* thread, uint8_t numaNode) {
	if (!::SetThreadAffinityMask(static_cast<HANDLE>(thread->native_handle()), getNumaNodeProcessorMask(numaNode))) {
		throw Exception();
	}
}

void runThreadOnProcessor(std::thread* thread, uint32_t processor) {
	uint32_t retval = ::SetThreadIdealProcessor(static_cast<HANDLE>(thread->native_handle()), processor);

	if (retval == static_cast<uint32_t>(-1)) {
		throw Exception();
	}
}

CpuGroupAffinity::CpuGroupAffinity() {
	initializeProcessorGroupInfo();
	getCurrentCpuGroup();
}

void CpuGroupAffinity::getCurrentCpuGroup() {
	GROUP_AFFINITY groupAffinity;
	if (!::GetThreadGroupAffinity(::GetCurrentThread(), &groupAffinity)) {
		throw Exception();
	}
	currentGroup_ = groupAffinity.Group;
}

void CpuGroupAffinity::setThreadAffinity(std::thread *thread) {
	GROUP_AFFINITY groupAffinity;

	bool found = false;
	DWORD minGroup = 0;

	for (auto i = 0; i < cpuGroups_.size(); i++) {
		auto minGroup = (currentGroup_ + i) % cpuGroups_.size();
		if (cpuGroups_[minGroup].activeThreadWeight / cpuGroups_[minGroup].groupWeight
			< (DWORD)cpuGroups_[minGroup].active) {
			found = true;
		}
	}

	if (!found) {
		minGroup = currentGroup_;
		auto minWeight = cpuGroups_[currentGroup_].activeThreadWeight;
		for (auto i = 0; i < cpuGroups_.size(); i++) {
			if (cpuGroups_[i].activeThreadWeight < minWeight) {
				minGroup = i;
				minWeight = cpuGroups_[i].activeThreadWeight;
			}
		}
	}

	groupAffinity.Group = minGroup;
	groupAffinity.Mask = cpuGroups_[minGroup].activeMask;
	groupAffinity.Reserved[0] = 0;
	groupAffinity.Reserved[1] = 0;
	groupAffinity.Reserved[2] = 0;

	cpuGroups_[minGroup].activeThreadWeight += cpuGroups_[minGroup].groupWeight;

	auto handle = static_cast<HANDLE>(thread->native_handle());
	if (!::SetThreadGroupAffinity(handle, &groupAffinity, nullptr)) {
		throw Exception();
	}
}

void CpuGroupAffinity::initializeProcessorGroupInfo() {
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pSLPIEx = nullptr;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRecord = nullptr;

	DWORD count = 0;
	::GetLogicalProcessorInformationEx(RelationGroup, pSLPIEx, &count);
	if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		throw Exception();
	}

	std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> buffer(count);
	pSLPIEx = buffer.data();
	if (!::GetLogicalProcessorInformationEx(RelationGroup, pSLPIEx, &count)) {
		throw Exception();
	}

	DWORD activeGroupCount = 0;
	pRecord = pSLPIEx;
	for (auto i = 0; i < count; ++i, ++pRecord) {
		if (pRecord->Relationship == RelationGroup) {
			activeGroupCount = pRecord->Group.ActiveGroupCount;
			break;
		}
	}

	cpuGroups_.resize(activeGroupCount);

	DWORD processorCount = 0;
	DWORD weight = 1;
	for (auto i = 0; i < activeGroupCount; ++i) {
		cpuGroups_[i].active = static_cast<WORD>(pRecord->Group.GroupInfo[i].ActiveProcessorCount);
		cpuGroups_[i].activeMask = pRecord->Group.GroupInfo[i].ActiveProcessorMask;
		processorCount += cpuGroups_[i].active;
		weight *= cpuGroups_[i].active;
	}

	for (auto i = 0; i < activeGroupCount; i++) {
		cpuGroups_[i].groupWeight = weight / cpuGroups_[i].active;
		cpuGroups_[i].activeThreadWeight = 0;
	}
}

}

}
