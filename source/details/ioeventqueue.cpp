//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/details/ioeventqueue.h>

namespace rapid {

namespace details {

IoEventQueue::IoEventQueue(uint32_t concurrentThreadCount) {
    handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, concurrentThreadCount);
    if (!handle) {
        throw Exception();
    }
}

IoEventQueue::~IoEventQueue() noexcept {
    if (handle != nullptr) {
        ::CloseHandle(handle);
    }
}

bool IoEventQueue::associateDevice(HANDLE device, ULONG_PTR compKey) const noexcept {
    return ::CreateIoCompletionPort(device, handle, compKey, 0) == handle;
}

bool IoEventQueue::enqueue(ULONG_PTR compKey, uint32_t numBytes, OVERLAPPED *overlapped) const noexcept {
    return ::PostQueuedCompletionStatus(handle, numBytes, compKey, overlapped) != FALSE;
}

}

}
