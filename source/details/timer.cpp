//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>

#include <rapid/details/timer.h>

namespace rapid {

namespace details {

#define MILLI_SECOND_TO_NANO100(x)  (x * 1000 * 10)

void CALLBACK Timer::onTimeoutCallback(PTP_CALLBACK_INSTANCE instance, PVOID param, PTP_TIMER handle) {
    auto timer = static_cast<Timer*>(param);
    timer->timeout();
}

Timer::Timer()
	: handle_(nullptr) {
    ::InitializeThreadpoolEnvironment(&callbackEnviron_);
	handle_ = ::CreateThreadpoolTimer(onTimeoutCallback, this, &callbackEnviron_);
	if (!handle_) {
        throw Exception(::GetLastError());
    }
}

Timer::~Timer() {
    stop();
}

void Timer::start(TimeoutCallback callback, uint32_t interval, bool immediately, bool once) {
    onTimeoutCallback_ = callback;
    FILETIME dueTime;
    *reinterpret_cast<PLONGLONG>(&dueTime) = -static_cast<LONGLONG>(MILLI_SECOND_TO_NANO100(0));
	::SetThreadpoolTimer(handle_, &dueTime, once ? 0 : interval, 0);
}

void Timer::stop() {
	if (handle_ != nullptr) {
		::SetThreadpoolTimer(handle_, nullptr, 0, 0);
		::WaitForThreadpoolTimerCallbacks(handle_, TRUE);
		::CloseThreadpoolTimer(handle_);
        ::DestroyThreadpoolEnvironment(&callbackEnviron_);
		handle_ = nullptr;
    }
}

void Timer::timeout() {
    onTimeoutCallback_();
}

}

}
