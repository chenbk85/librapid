//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <cstdint>

#include <rapid/details/memallocator.h>

namespace rapid {

namespace details {

class VMemAllocator : public MemAllocator {
public:
    VMemAllocator(uint32_t size, bool useLargePages);

    virtual ~VMemAllocator();

    virtual void query(char *addr, MEMORY_BASIC_INFORMATION *info) override;

    virtual char * commit(char *addr, uint32_t size) override;

    virtual void protect(char *addr, uint32_t size) override;

    virtual void decommit(char *addr, uint32_t size) override;
};

}

}