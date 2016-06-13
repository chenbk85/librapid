//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <map>

#include <rapid/utils/singleton.h>
#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class DLLMap : public utils::Singleton<DLLMap> {
public:
    ~DLLMap();

    DLLMap(DLLMap const &) = delete;
    DLLMap& operator=(DLLMap const &) = delete;

    LPVOID getProcAddress(std::string const &dllName, std::string const &functionName);
private:
    friend class utils::Singleton<DLLMap>;
    DLLMap();

    std::map<std::string, HMODULE> moduleMap_;
};

template <typename R, typename... T>
class DLLAPI final {
public:
    DLLAPI(std::string const &dllName, std::string const &functionName) {
        pDllPfn_ = static_cast<R(__stdcall * const)(T...)>(
			DLLMap::getInstance().getProcAddress(dllName, functionName.c_str()));
    }

	__forceinline bool valid() noexcept {
		return pDllPfn_ != nullptr;
	}

    __forceinline R operator()(T... args) noexcept {
        return (*pDllPfn_)(args...);
    }

private:
    R(__stdcall * const pDllPfn_)(T...);
};

}

}
