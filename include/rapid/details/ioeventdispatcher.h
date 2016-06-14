//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

#include <rapid/utils/singleton.h>
#include <rapid/platform/platform.h>

namespace rapid {

namespace details {

class IoEventQueue;
class IoEventDispatcher;

class IoEventDispatcher : public utils::Singleton<IoEventDispatcher> {
public:
    explicit IoEventDispatcher(uint32_t concurrentThreadCount);

	IoEventDispatcher();

	IoEventDispatcher(IoEventDispatcher &&other);

	IoEventDispatcher & operator=(IoEventDispatcher &&other);

	~IoEventDispatcher();

    IoEventDispatcher(IoEventDispatcher const &) = delete;
    IoEventDispatcher& operator=(IoEventDispatcher const &) = delete;

    void addDevice(HANDLE device, ULONG_PTR compKey) const;

    void waitForIoLoop(uint32_t timeout) const;

    void postQuit() const;

	void post(ULONG_PTR compKey, OVERLAPPED *overlapped) const;

private:
	static auto constexpr KEY_IO_SERVICE_STOP = MAXULONG_PTR;
	static auto constexpr MAX_OVERLAPPED_ENTRIES = 128;
    
    std::unique_ptr<IoEventQueue> pIoEventQueue_;
};

}

}
