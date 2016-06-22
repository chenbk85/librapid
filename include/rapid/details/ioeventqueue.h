//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include <rapid/platform/platform.h>

namespace rapid {

namespace details {

class IoEventQueue {
public:
    explicit IoEventQueue(uint32_t concurrentThreadCount);

    IoEventQueue(IoEventQueue const &) = delete;
    IoEventQueue& operator=(IoEventQueue const &) = delete;

    ~IoEventQueue() noexcept;

    bool associateDevice(HANDLE device, ULONG_PTR compKey) const noexcept;

    bool enqueue(ULONG_PTR compKey, uint32_t numBytes, OVERLAPPED *overlapped) const noexcept;

    bool dequeue(OVERLAPPED_ENTRY * __restrict entries, DWORD N, ULONG * __restrict removeCount, uint32_t timeout) const noexcept;

private:
    HANDLE handle;
};

__forceinline bool IoEventQueue::dequeue(OVERLAPPED_ENTRY * __restrict entries, DWORD N, ULONG * __restrict removeCount, uint32_t timeout) const noexcept {
    return ::GetQueuedCompletionStatusEx(handle, entries, N, removeCount, timeout, FALSE) != FALSE;
}

}

}