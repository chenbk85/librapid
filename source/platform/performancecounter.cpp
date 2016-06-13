//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma comment(lib, "Pdh.lib")

#include <sstream>

#include <rapid/exception.h>

#include <rapid/platform/registry.h>
#include <rapid/platform/performancecounter.h>

namespace rapid {

namespace platform {

enum {
    PERF_COUNTER_NOTSUPPORTED = 0,
    PERF_COUNTER_INITIALIZED,
    PERF_COUNTER_GET_SECOND_VALUE,
    PERF_COUNTER_ACTIVE,
};

static inline PPERF_OBJECT_TYPE getBeginPperfObject(PPERF_DATA_BLOCK perfData) {
	return reinterpret_cast<PPERF_OBJECT_TYPE>(reinterpret_cast<PBYTE>(perfData) + perfData->HeaderLength);
}

static inline PPERF_COUNTER_DEFINITION getNextPperfObjectCounter(PPERF_OBJECT_TYPE object) {
	return reinterpret_cast<PPERF_COUNTER_DEFINITION>(reinterpret_cast<PBYTE>(object) + object->HeaderLength);
}

static inline PPERF_OBJECT_TYPE getNextPperfObject(PPERF_OBJECT_TYPE object) {
	return reinterpret_cast<PPERF_OBJECT_TYPE>(reinterpret_cast<PBYTE>(object) + object->TotalByteLength);
}

static inline PPERF_INSTANCE_DEFINITION getNextPperfInstance(PPERF_OBJECT_TYPE object) {
	return reinterpret_cast<PPERF_INSTANCE_DEFINITION>(reinterpret_cast<PBYTE>(object) + object->DefinitionLength);
}

PerformanceCounter::PerformanceCounter()
	: query_(nullptr)
	, counter_(nullptr) {
}

PerformanceCounter::PerformanceCounter(std::wstring const &categoryName, std::wstring const &counterName, std::wstring const &instanceName)
    : PerformanceCounter() {
	start(categoryName, counterName, instanceName);
}

PerformanceCounter::~PerformanceCounter() {
    stop();
}

void PerformanceCounter::start(std::wstring const &categoryName, std::wstring const &counterName, std::wstring const &instanceName) {
	std::wostringstream ostr;

	if (!instanceName.empty()) {
		ostr << L"\\" << categoryName << L"(" << instanceName << L")\\" << counterName;
	}
	else {
		ostr << L"\\" << categoryName << L"\\" << counterName;
	}

	path_ = ostr.str();

    stop();
    
	if (::PdhOpenQuery(nullptr, 0, &query_) != ERROR_SUCCESS) {
        throw Exception();
    }
    
	auto retval = ::PdhValidatePath(path_.c_str());
    if (retval != ERROR_SUCCESS) {
        throw Exception(retval);
    }

    if (::PdhAddCounter(query_, path_.c_str(), 0, &counter_) != ERROR_SUCCESS) {
        throw Exception();
    }
}

void PerformanceCounter::stop() {
    if (counter_ != nullptr) {
        ::PdhRemoveCounter(counter_);
        counter_ = nullptr;
    }

    if (counter_ != nullptr) {
        ::PdhCloseQuery(query_);
        query_ = nullptr;
    }
}

PDH_FMT_COUNTERVALUE PerformanceCounter::current(uint32_t format) const {
	PDH_FMT_COUNTERVALUE value = { 0 };
    
    while (true) {
        if (::PdhCollectQueryData(query_) != ERROR_SUCCESS) {
            throw Exception();
        }

        auto retval = ::PdhGetFormattedCounterValue(counter_, format, nullptr, &value);
        if (retval != ERROR_SUCCESS) {
            if (value.CStatus == PDH_CSTATUS_INVALID_DATA) {
                value.CStatus = PERF_COUNTER_GET_SECOND_VALUE;
                continue;
            }
            throw Exception();
        } else {
            break;
        }
    }
    return value;
}

LONG PerformanceCounter::currentAsLong() const {
    return current(PDH_FMT_LONG).longValue;
}

double PerformanceCounter::currentAsDouble() const {
    return current(PDH_FMT_DOUBLE).doubleValue;
}

std::vector<std::wstring> PerformanceCounter::getCounterName() {
	std::vector<std::wstring> counterNames;

	Registry counters;

	auto data = counters.getBinary(HKEY_PERFORMANCE_DATA, L"Counters");

	for (auto p = reinterpret_cast<wchar_t*>(data.data()); *p; p += (wcslen(p) + 1)) {
		auto counter = _wcstoi64(p, nullptr, 10);
		p += (wcslen(p) + 1);
		counterNames.emplace_back(p);
	}

	return counterNames;
}

std::vector<std::wstring> PerformanceCounter::getInstanceName() {
	std::vector<std::wstring> instanceNames;

	Registry global;
	
	auto data = global.getBinary(HKEY_PERFORMANCE_DATA, L"Global");

	auto pDataBlock = reinterpret_cast<PPERF_DATA_BLOCK>(data.data());
	auto pObject = getBeginPperfObject(pDataBlock);

	instanceNames.reserve(pDataBlock->NumObjectTypes);

	for (int i = 0; i < pDataBlock->NumObjectTypes; ++i, pObject = getNextPperfObject(pObject)) {
		if (pObject->NumInstances == PERF_NO_INSTANCES) {
			continue;
		}

		auto pInstance = getNextPperfInstance(pObject);
		for (int j = 0; j < pObject->NumInstances; j++) {
			if (!pInstance->NameLength) {
				continue;
			}
			auto str = reinterpret_cast<wchar_t*>((reinterpret_cast<PBYTE>(pInstance) + pInstance->NameOffset));
			auto len = pInstance->NameLength - sizeof(wchar_t);
			instanceNames.emplace_back(str, len);
		}
	}

	return instanceNames;
}

std::wstring PerformanceCounter::path() const {
	return path_;
}

}

}
