//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include <Pdh.h>
#include <PdhMsg.h>

namespace rapid {

namespace platform {

class PerformanceCounter {
public:
	PerformanceCounter();

	PerformanceCounter(std::wstring const &categoryName, std::wstring const &counterName, std::wstring const &instanceName = L"");

    PerformanceCounter(PerformanceCounter const &) = delete;
    PerformanceCounter& operator=(PerformanceCounter const &) = delete;

    ~PerformanceCounter();

    void start(std::wstring const &categoryName, std::wstring const &counterName, std::wstring const &instanceNmae = L"");

    void stop();

    LONG currentAsLong() const;

    double currentAsDouble() const;

	static std::vector<std::wstring> getCounterName();

	static std::vector<std::wstring> getInstanceName();

	std::wstring path() const;
private:
    PDH_FMT_COUNTERVALUE current(uint32_t format) const;

    HQUERY query_;
    HCOUNTER counter_;
	std::wstring path_;
};


}

}
