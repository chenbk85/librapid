//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/connection.h>
#include <rapid/iobuffer.h>

#include <rapid/logging/logging.h>

#include <rapid/details/contracts.h>
#include <rapid/details/ioeventqueue.h>
#include <rapid/details/ioeventdispatcher.h>

namespace rapid {

namespace details {

IoEventDispatcher::IoEventDispatcher(uint32_t concurrentThreadCount)
    : pIoEventQueue_(std::make_unique<IoEventQueue>(concurrentThreadCount)) {
}

IoEventDispatcher::IoEventDispatcher() {
}

IoEventDispatcher::IoEventDispatcher(IoEventDispatcher && other) {
	*this = std::move(other);
}

IoEventDispatcher::~IoEventDispatcher() {
}

void IoEventDispatcher::addDevice(HANDLE device, ULONG_PTR compKey) const {
    if (!pIoEventQueue_->associateDevice(device, compKey)) {
        throw Exception();
    }
}

void IoEventDispatcher::post(ULONG_PTR compKey, OVERLAPPED *overlapped) const {
	if (!pIoEventQueue_->enqueue(compKey, -1, overlapped)) {
		throw Exception();
	}
}

IoEventDispatcher & IoEventDispatcher::operator=(IoEventDispatcher && other) {
	if (this != &other) {
		pIoEventQueue_ = std::move(other.pIoEventQueue_);
	}
	return *this;
}

void IoEventDispatcher::postQuit() const {
    if (!pIoEventQueue_->enqueue(KEY_IO_SERVICE_STOP, 0, nullptr)) {
        throw Exception();
    }
}

void IoEventDispatcher::waitForIoLoop(uint32_t timeout) const {
	RAPID_LOG_TRACE_STACK_TRACE();

	// NOTE: 
	// IOCP 並行的機會發生在此!
	// TCP是全雙工的, 但是程式無法並行去處理, 所以最佳的做法是讓上層把buffer填滿,
	// 並透過IOCP並行的消耗buffer!

    OVERLAPPED_ENTRY entries[MAX_OVERLAPPED_ENTRIES];
    ULONG removeCount;

    for (;;) {
        auto retval = pIoEventQueue_->dequeue(entries, MAX_OVERLAPPED_ENTRIES, &removeCount, timeout);
        
		if (!retval) {
            // NOTICE: This function returns FALSE when no I/O operation was dequeued.
            continue;
        }
		
		RAPID_LOG_TRACE() << "Remove " << removeCount << " IO completion packet";
        
		for (ULONG i = 0; i < removeCount; ++i) {
			Connection *pConn = nullptr;
			IoBuffer *pBuffer = nullptr;
            if (entries[i].lpCompletionKey == 0) {
                // Accept new connection.
				pConn = static_cast<Connection*>(entries[i].lpOverlapped);
            } else {
                if (entries[i].lpCompletionKey == KEY_IO_SERVICE_STOP) {
                    RAPID_LOG_INFO() << "Worker thread stopping...";
                    return;
                }			
				pConn = reinterpret_cast<Connection*>(entries[i].lpCompletionKey);
				pBuffer = static_cast<IoBuffer*>(entries[i].lpOverlapped);
            }
			RAPID_LOG_TRACE() << entries[i].dwNumberOfBytesTransferred << " bytes transferred ";
			pConn->onIoCompletion(pBuffer, entries[i].dwNumberOfBytesTransferred);
        }
    }
}

}

}
