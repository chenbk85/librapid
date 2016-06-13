//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <thread>

#include <rapid/platform/platform.h>

namespace rapid {

namespace utils {

struct RaceDetector {
	volatile std::atomic_flag entered = ATOMIC_FLAG_INIT;
};

class RaceDetectGuard {
public:
	RaceDetectGuard(RaceDetector &detector) noexcept
		: detector_(detector) {
		if (detector_.entered.test_and_set(std::memory_order_acquire) == true) {
			__debugbreak();
		}
	}

	RaceDetectGuard(RaceDetectGuard const &) = delete;
	RaceDetectGuard& operator=(RaceDetectGuard const &) = delete;

	~RaceDetectGuard() noexcept {
		detector_.entered.clear(std::memory_order_release);
	}

private:
	RaceDetector &detector_;
};

}

}
