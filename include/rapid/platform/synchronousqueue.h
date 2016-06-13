//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <exception>

#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class CouldNotEnterException : public std::exception {};
class NoPairedCallException : public std::exception {};

template <typename T>
class SynchronousQueue {
public:
	explicit SynchronousQueue(DWORD waitMS)
		: wait_{ waitMS }
		, address_{ nullptr } {
		::InitializeCriticalSection(&getCriticalSection_);
		::InitializeCriticalSection(&putCriticalSection_);

		getSemaphore_ = ::CreateSemaphore(nullptr, 0, 1, nullptr);
		putSemaphore_ = ::CreateSemaphore(nullptr, 0, 1, nullptr);
	}

	~SynchronousQueue() {
		::EnterCriticalSection(&getCriticalSection_);
		::EnterCriticalSection(&putCriticalSection_);

		::CloseHandle(getSemaphore_);
		::CloseHandle(putSemaphore_);

		::DeleteCriticalSection(&putCriticalSection_);
		::DeleteCriticalSection(&getCriticalSection_);
	}

	void put(const T& value) {
		if (!::TryEnterCriticalSection(&putCriticalSection_)) {
			throw CouldNotEnterException();
		}

		::ReleaseSemaphore(putSemaphore_, static_cast<LONG>(1), nullptr);

		if (::WaitForSingleObject(getSemaphore_, wait_) != WAIT_OBJECT_0) {
			if (::WaitForSingleObject(putSemaphore_, 0) == WAIT_OBJECT_0) {
				::LeaveCriticalSection(&putCriticalSection_);
				throw NoPairedCallException();
			} else {
				::WaitForSingleObject(getSemaphore_, 0);
			}
		}

		address_ = &value;
		valueReady_ = true;
		while (valueReady_) {
		}

		::LeaveCriticalSection(&putCriticalSection_);
	}

	T get() {
		if (!::TryEnterCriticalSection(&getCriticalSection_)) {
			throw CouldNotEnterException();
		}

		::ReleaseSemaphore(getSemaphore_, static_cast<LONG>(1), nullptr);

		if (::WaitForSingleObject(putSemaphore_, wait_) != WAIT_OBJECT_0) {
			if (::WaitForSingleObject(getSemaphore_, 0) == WAIT_OBJECT_0) {
				::LeaveCriticalSection(&getCriticalSection_);
				throw NoPairedCallException();
			} else {
				::WaitForSingleObject(putSemaphore_, 0);
			}
		}

		while (!valueReady_) {
		}

		T toReturn = *address_;
		valueReady_ = false;

		::LeaveCriticalSection(&getCriticalSection_);
		return toReturn;
	}
private:
	std::atomic_bool valueReady_{ false };

	CRITICAL_SECTION getCriticalSection_;
	CRITICAL_SECTION putCriticalSection_;

	DWORD wait_{ 0 };

	HANDLE getSemaphore_;
	HANDLE putSemaphore_;

	const T* address_{ nullptr };
};

}

}
