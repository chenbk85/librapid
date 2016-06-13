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

class SystemInfo : public utils::Singleton<SystemInfo> {
public:
    SystemInfo();

    ~SystemInfo() noexcept;

    uint32_t getNumberOfProcessors(uint32_t perCPU) const noexcept;

    uint32_t getPageSize() const noexcept;

	uint32_t getPageBoundarySize() const noexcept;

	uint32_t roundUpToPageSize(uint32_t size) const noexcept;

private:
    class SystemInfoImpl;
    std::unique_ptr<SystemInfoImpl> pInfo_;
};

bool startupWinSocket();

std::wstring getApplicationFilePath();

std::wstring getApplicationFileName();

void setProcessPriorityBoost(bool enableBoost);

std::vector<uint32_t> enumThreadIds();

int64_t getFileSize(std::string const &filePath);

void prefetchVirtualMemory(char const * virtualAddress, size_t size);

}

}

