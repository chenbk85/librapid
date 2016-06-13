//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class Registry {
public:
    Registry();

    Registry(HKEY key, std::wstring const &section);

    ~Registry();

	static std::unique_ptr<Registry> createLocalMachineBranch(std::wstring const &section);

	static void deleteLocalMachineBranch(std::wstring const &section);

    void setRegistryBranch(HKEY key, std::wstring const &section);

	void setDword(std::wstring const &entry, int value);

    uint32_t getDword(std::wstring const &entry);

	void setString(std::wstring const &entry, std::wstring const &value);

    std::wstring getString(std::wstring const &entry, std::wstring const &strDefault);

    std::vector<BYTE> getBinary(HKEY branch, std::wstring const &section);

private:
    std::wstring getString(HKEY branch,
                           std::wstring const &section,
                           std::wstring const &entry,
                           std::wstring const &strDefault);

	void setString(HKEY branch,
		std::wstring const &section,
		std::wstring const &entry,
		std::wstring const &value);

	void setDword(HKEY branch, std::wstring const &section, std::wstring const &entry, int value);

    uint32_t getDword(HKEY branch, std::wstring const &section, std::wstring const &entry);

    HKEY baseKey_;
    std::wstring section_;
};

}

}
