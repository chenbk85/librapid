//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <future>
#include <type_traits>

#include <rapid/details/contracts.h>

#include <rapid/utils/mpmc_bounded_queue.h>

// https://github.com/inkooboo/thread-pool-cpp

namespace rapid {

namespace utils {

namespace details {

inline static uint32_t * getCurrentThreadId() {
	static thread_local uint32_t id = UINT32_MAX;
	return &id;
}

class Worker {
public:
	explicit Worker(uint32_t queueSize);

	~Worker();

	Worker(Worker const &) = delete;
	Worker& operator=(Worker const &) = delete;

	void start(uint32_t id, std::weak_ptr<Worker> pPair);

	template <typename Handler>
	bool tryPost(Handler &&handler) noexcept;

	bool trySteal(std::function<void(void)> &handler) noexcept;
private:
	volatile bool stopped_;
	std::thread thread_;
	mpmc_bounded_queue<std::function<void(void)>> queue_;
};

inline Worker::Worker(uint32_t queueSize)
	: stopped_(false)
	, queue_(queueSize) {
}

Worker::~Worker() {
	stopped_ = true;
	if (thread_.joinable())
		thread_.join();
}

inline void Worker::start(uint32_t id, std::weak_ptr<Worker> pPair) {
	thread_ = std::thread([this, pPair, id]() {
		*getCurrentThreadId() = id;

		std::function<void(void)> handler;

		while (!stopped_) {
			auto pOther = pPair.lock();

			if (!pOther)
				continue;
			
			if (queue_.dequeue(handler) || pOther->trySteal(handler)) {
				try {
					handler(); 
				} catch (...) {
				}
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	});
}

inline bool Worker::trySteal(std::function<void(void)>& handler) noexcept {
	return queue_.dequeue(handler);
}

template<typename Handler>
inline bool Worker::tryPost(Handler &&handler) noexcept {
	return queue_.enqueue(std::forward<Handler>(handler));
}

}

class ThreadPool {
public:
	static uint32_t constexpr DEFAULT_NUM_WORKER = 4096;

	ThreadPool(uint32_t numWorker = DEFAULT_NUM_WORKER);

	ThreadPool(ThreadPool const &) = delete;
	ThreadPool& operator=(ThreadPool const &) = delete;

	~ThreadPool();

	template <typename Handler>
	void excute(Handler &&handler);

	template <typename Handler, typename R = typename std::result_of<Handler()>::type>
	typename std::future<R> async(Handler &&handler);
private:
	details::Worker & getNextWorker();

	std::atomic<uint32_t> index_;
	std::vector<std::shared_ptr<details::Worker>> workers_;
};

inline ThreadPool::ThreadPool(uint32_t numWorker) {
	workers_.resize(std::thread::hardware_concurrency());

	for (auto &pWorker : workers_) {
		pWorker.reset(new details::Worker(numWorker));
	}

	for (size_t i = 0; i < workers_.size(); ++i) {
		workers_[i]->start(i, 
			std::weak_ptr<details::Worker>(workers_[(i + 1) % workers_.size()]));
	}
}

inline ThreadPool::~ThreadPool() {
}

inline details::Worker & ThreadPool::getNextWorker() {
	auto id = *details::getCurrentThreadId();
	if (id > workers_.size()) {
		id = index_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
	}
	return *workers_[id];
}

template<typename Handler>
inline void ThreadPool::excute(Handler && handler) {
	if (!getNextWorker().tryPost(std::forward<Handler>(handler))) {
		throw std::overflow_error("worker queue is full");
	}
}

template <typename Handler, typename R>
typename std::future<R> ThreadPool::async(Handler &&handler) {
	std::packaged_task<R()> task([handler = std::move(handler)]() {
		return handler();
	});

	std::future<R> result = task.get_future();

	if (!getNextWorker().tryPost(task)) {
		throw std::overflow_error("worker queue is full");
	}

	return result;
}

}


}
