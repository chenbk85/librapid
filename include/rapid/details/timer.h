//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <functional>

#include <rapid/platform/platform.h>

namespace rapid {

namespace details {

class Timer;

using TimerPtr = std::shared_ptr<Timer>;

class Timer {
public:
    using TimeoutCallback = std::function<void(void)>;

    static TimerPtr createTimer();

    Timer();

    ~Timer();

    Timer(Timer const &) = delete;
    Timer& operator=(Timer const &) = delete;

    void start(TimeoutCallback callback, uint32_t interval, bool immediately = true, bool once = false);

    void stop();
private:
    void timeout();

    static void CALLBACK onTimeoutCallback(PTP_CALLBACK_INSTANCE instance, PVOID param, PTP_TIMER timer);

    TimeoutCallback onTimeoutCallback_;
    TP_CALLBACK_ENVIRON callbackEnviron_;
    PTP_TIMER handle_;
};

inline TimerPtr Timer::createTimer() {
    return std::make_shared<Timer>();
}

}

}
