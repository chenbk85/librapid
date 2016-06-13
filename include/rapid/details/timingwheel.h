//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <rapid/platform/spinlock.h>
#include <rapid/details/timer.h>

namespace rapid {

namespace details {

class Timer;

class TimingWheel;
using TimingWheelPtr = std::shared_ptr<TimingWheel>;

class TimingWheel {
public:
    using TimeoutCallback = std::function<void(void)>;

	static TimingWheelPtr createTimingWheel(uint32_t tickDuration, uint32_t maxBuckets);

	TimingWheel(uint32_t tickDuration, uint32_t maxBuckets);

    ~TimingWheel();

    TimingWheel(TimingWheel const &) = delete;
    TimingWheel& operator=(TimingWheel const &) = delete;

    void start();

	uint64_t add(uint32_t timeout, bool onece, TimeoutCallback callback);

    void remove(uint64_t slot);

private:
    void onTick();

    union Slot {
        uint64_t pad;
        uint32_t value[2];
    };

    using Bucket = std::vector<std::pair<bool, TimeoutCallback>>;

	details::Timer timer_;
    std::vector<Bucket> buckets_;
    mutable platform::Spinlock lock_;
    uint64_t maxTimeout_;
	uint32_t tickDuration_;
    uint32_t nextIndex_;
};

}

}
