//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <mutex>

#include <rapid/details/contracts.h>
#include <rapid/details/timingwheel.h>

namespace rapid {

namespace details {

static void defaultTimeoutCallback() {
}

TimingWheelPtr TimingWheel::createTimingWheel(uint32_t tickDuration, uint32_t maxBuckets) {
	return std::make_shared<TimingWheel>(tickDuration, maxBuckets);
}

TimingWheel::TimingWheel(uint32_t tickDuration, uint32_t maxBuckets)
    : buckets_(maxBuckets)
	, tickDuration_(tickDuration)
    , nextIndex_(0) {
	maxTimeout_ = tickDuration_ * maxBuckets;
}

TimingWheel::~TimingWheel() {
	std::lock_guard<platform::Spinlock> guard{ lock_ };
    timer_.stop();
}

void TimingWheel::start() {
	timer_.start(std::bind(&TimingWheel::onTick, this), tickDuration_);
}

uint64_t TimingWheel::add(uint32_t timeout, bool onece, TimeoutCallback callback) {
	std::lock_guard<platform::Spinlock> guard{ lock_ };
    
	RAPID_ENSURE(timeout <= maxTimeout_);

	auto bucket = (nextIndex_ + timeout / tickDuration_) % buckets_.size();
	auto id = uint32_t(buckets_[bucket].size());
    
	buckets_[bucket].push_back(std::make_pair(onece, callback));
    
	Slot s;
    s.value[0] = bucket;
    s.value[1] = id;
    return s.pad;
}

void TimingWheel::onTick() {
	std::lock_guard<platform::Spinlock> guard{ lock_ };

    auto & bucket = buckets_[nextIndex_];
    for (auto &callback : bucket) {
		if (callback.second != nullptr) {
			callback.second();
			if (callback.first) {
				callback.second = nullptr;
			}
		}
    }

    nextIndex_ = (nextIndex_ + 1) % buckets_.size();
}

void TimingWheel::remove(uint64_t slot) {
	std::lock_guard<platform::Spinlock> guard{ lock_ };

    Slot s;
    s.pad = slot;
    auto bucket = s.value[0];
    auto &chain = buckets_[bucket];
    
	RAPID_ENSURE(s.value[1] < chain.size());
    chain[s.value[1]].second = nullptr;
}

}

}
