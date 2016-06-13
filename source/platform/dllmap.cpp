//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/platform/dllmap.h>

namespace rapid {

namespace platform {

DLLMap::DLLMap() {

}

DLLMap::~DLLMap() {
    for (auto module : moduleMap_) {
        ::FreeLibrary(module.second);
    }
}

LPVOID DLLMap::getProcAddress(std::string const &dllName, std::string const &functionName) {
    if (moduleMap_.find(dllName) == moduleMap_.end()) {
        auto module = ::LoadLibraryA(dllName.c_str());
        moduleMap_[dllName] = module;
    }
    auto module = moduleMap_[dllName];
    LPVOID fp = ::GetProcAddress(module, functionName.c_str());
    return fp;
}

}

}
