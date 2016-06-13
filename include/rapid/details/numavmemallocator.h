//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <rapid/platform/platform.h>

#include <rapid/details/memallocator.h>

namespace rapid {

namespace details {

class NumaVMemAllocator : public MemAllocator {
public:
    NumaVMemAllocator(UCHAR node, uint32_t size, bool useLargePages);

    ~NumaVMemAllocator() override;

    virtual void query(char *addr, MEMORY_BASIC_INFORMATION *info) override;

    virtual char * commit(char *addr, uint32_t size) override;

    virtual void protect(char *addr, uint32_t size) override;

    virtual void decommit(char *addr, uint32_t size) override;
private:
    UCHAR numaNode_;
    HANDLE currentProcess_;
};

}

}
