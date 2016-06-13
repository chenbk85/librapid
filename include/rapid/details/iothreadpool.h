//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <thread>
#include <memory>
#include <vector>

#include <atomic>

namespace rapid {

namespace details {

class IoThreadPool {
public:
	explicit IoThreadPool(uint32_t threadCount);

    IoThreadPool(IoThreadPool const &) = delete;
    IoThreadPool& operator=(IoThreadPool const &) = delete;

    ~IoThreadPool();

    void start();

    void joinAll();

    std::vector<std::thread> const & threads() const;

private:
	static auto constexpr MAX_START_THREAD_TIME_EXPIRED = 40000;
	static auto constexpr CACHE_LINE_PAD_SIZE = 128;
	static auto constexpr MAX_CACHE_LINE_PAD_SIZE = 4096;

	void ensureStarted();
    uint32_t numThreads_;
	std::atomic<int> padCacheLineSizeCount_;
	std::atomic<int> startedThreadCount_;
    std::vector<std::thread> pool_;
};

}

}
