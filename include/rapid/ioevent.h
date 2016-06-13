//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

#include <rapid/platform/platform.h>

namespace rapid {

namespace details {
class IoEventLoop;
}

class IoBuffer;

class IoEvent : public OVERLAPPED {
public:
	virtual ~IoEvent() = default;

	IoEvent(IoEvent const &) = delete;
	IoEvent& operator=(IoEvent const &) = delete;

	void resetOverlappedValue() throw() {
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		Pointer = nullptr;
		hEvent = nullptr;
	}

	void onIoCompletion(std::shared_ptr<details::IoEventLoop> loop, IoBuffer *pBuffer, uint32_t bytesTransferred) {
		return onCompletion(loop, pBuffer, bytesTransferred);
	}

protected:
	IoEvent() throw() {
		resetOverlappedValue();
	}

	virtual void onCompletion(std::shared_ptr<details::IoEventLoop> &loop,
		IoBuffer *pBuffer,
		uint32_t bytesTransferred) = 0;
};

}
