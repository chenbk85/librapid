//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/logging/logging.h>

#include <rapid/details/contracts.h>
#include <rapid/platform/utils.h>

#include <rapid/details/ioeventdispatcher.h>
#include <rapid/platform/threadutils.h>
#include <rapid/details/iothreadpool.h>

namespace rapid {

namespace details {

IoThreadPool::IoThreadPool(uint32_t threadCount)
    : numThreads_(threadCount)
	, padCacheLineSizeCount_(0)
	, startedThreadCount_(0) {
    RAPID_ENSURE(threadCount > 0);
}

IoThreadPool::~IoThreadPool() {
    joinAll();
}

void IoThreadPool::runAll() {
    static auto constexpr WAIT_IO_COMPLETION_TIMEOUT = 15000;

    for (uint32_t i = 0; i < numThreads_; ++i) {
        pool_.emplace_back([i, this]() {
			++startedThreadCount_;

			// Fix cache-line (64kb / 1mb aliasing) aliasing problem in hyper-threading cpu 
			++padCacheLineSizeCount_;
			if (padCacheLineSizeCount_ * CACHE_LINE_PAD_SIZE > MAX_CACHE_LINE_PAD_SIZE)
				padCacheLineSizeCount_ = 0;

			_alloca(padCacheLineSizeCount_ * CACHE_LINE_PAD_SIZE);

			try {
				details::IoEventDispatcher::getInstance().waitForIoLoop(WAIT_IO_COMPLETION_TIMEOUT);
				// 任何一個Thread結束的時候都多發送一個結束Key(KEY_IO_SERVICE_STOP), 可以避免一個Thread處理多個結束Key(KEY_IO_SERVICE_STOP).
				details::IoEventDispatcher::getInstance().postQuit();
			} catch (std::exception const &e) {
				RAPID_LOG_FATAL() << e.what();
			} catch (...) {
				RAPID_LOG_FATAL() << "Uknown catch exception";
			}
        });

        std::ostringstream ostr;
        ostr << "Worker " << i << " thread";
        auto const threadName = ostr.str();
        platform::setThreadName(&pool_[i], threadName.c_str());
    }

	ensureStarted();
}

void IoThreadPool::ensureStarted() {
	int loopCount = 0;

	while (startedThreadCount_ != pool_.size()) {
		++loopCount;
		if (loopCount >= MAX_START_THREAD_TIME_EXPIRED) {
			throw Exception("Thread pool start timeout");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void IoThreadPool::joinAll() {
	details::IoEventDispatcher::getInstance().postQuit();

    for (auto &thread : pool_) {
        if (thread.joinable())
            thread.join();
    }

    pool_.clear();
}

std::vector<std::thread> const & IoThreadPool::threads() const {
    return pool_;
}

}

}



