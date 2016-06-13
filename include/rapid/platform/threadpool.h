//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <condition_variable>

#include <concurrent_queue.h>

#include <rapid/details/contracts.h>
#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class Environment {
public:
	Environment() throw();

	Environment(Environment const &) = delete;
	Environment& operator=(Environment const &) = delete;

	~Environment() throw();

	PTP_CALLBACK_ENVIRON get() throw() {
		return &environ_;
	}

private:
	TP_CALLBACK_ENVIRON environ_;
};

class CallbackInstance {
public:
	CallbackInstance();

	bool isMayLongRunning() const throw();

	void disassociate() throw();

	void setEventWhenTaskReturns(HANDLE event) throw();

protected:
	PTP_CALLBACK_INSTANCE instance_;
};

class CleanupGroup {
public:
	CleanupGroup() throw();

	CleanupGroup(CleanupGroup const &) = delete;
	CleanupGroup& operator=(CleanupGroup const &) = delete;

	~CleanupGroup() throw();

	PTP_CLEANUP_GROUP get() const throw() {
		return pCleanupGroup_;
	}

private:
	PTP_CLEANUP_GROUP pCleanupGroup_;
};

class WaitableTask : public CallbackInstance {
public:
	WaitableTask(std::function<void(void)> action, Environment &env);

	WaitableTask(WaitableTask const &) = delete;
	WaitableTask& operator=(WaitableTask const &) = delete;

	~WaitableTask() throw();

	void run(HANDLE waitForObject, uint32_t timeoutAsMs);

	void cancel();

private:
	FILETIME * createTimeout() throw();

	static void CALLBACK callback(PTP_CALLBACK_INSTANCE instance,
		PVOID param,
		PTP_WAIT wait,
		TP_WAIT_RESULT waitResult);

	std::function<void(void)> action_;
	uint32_t timeoutAsMs_;
	FILETIME waitFiletime_;
	HANDLE waitForObject_;
	PTP_WAIT pWait_;
};

class WorkableTask : public CallbackInstance {
public:
	explicit WorkableTask(Environment &environment);

	WorkableTask(std::function<void(void)> eventHandler, Environment &environment);

	WorkableTask(WorkableTask const &) = delete;
	WorkableTask& operator=(WorkableTask const &) = delete;

	~WorkableTask() throw();

	void run(std::function<void(void)> action);

	void run() const;

	void join() const;

	void cancel() const;

private:
	static void CALLBACK callback(PTP_CALLBACK_INSTANCE instance, PVOID ctx, PTP_WORK work);
	std::function<void(void)> action_;
	PTP_WORK pWork_;
};

}

template <typename T>
class BaseThreadPool {
public:
	using TaskPtr = std::shared_ptr<T>;

	// Windows default thread pool size.
	enum : uint32_t {
		THREAD_POOL_SIZE_MIN = 1,
		THREAD_POOL_SIZE_MAX = 500
	};

	enum TaskPrioritys {
		PRIORITY_HIGH = TP_CALLBACK_PRIORITY_HIGH,
		PRIORITY_LOW = TP_CALLBACK_PRIORITY_LOW,
		PRIORITY_NORMAL = TP_CALLBACK_PRIORITY_NORMAL
	};

	BaseThreadPool()
		: pPool_(::CreateThreadpool(nullptr), &::CloseThreadpool) {
		RAPID_ENSURE(pPool_ != nullptr);
		::SetThreadpoolCallbackCleanupGroup(environment_.get(), cleanupGroup_.get(), nullptr);
	}

	virtual ~BaseThreadPool() = default;

	BaseThreadPool(uint32_t minimumPoolSize, uint32_t maximumPoolSize) 
		: BaseThreadPool() {
		setPoolSize(minimumPoolSize, maximumPoolSize);
	}

	BaseThreadPool(BaseThreadPool const &) = delete;
	BaseThreadPool& operator=(BaseThreadPool const &) = delete;

	virtual void start(std::function<void(void)> action) = 0;

	void setPoolSize(uint32_t minimumPoolSize, uint32_t maximumPoolSize) {
		if (!::SetThreadpoolThreadMinimum(pPool_.get(), minimumPoolSize)) {
			throw Exception();
		}
		::SetThreadpoolThreadMaximum(pPool_.get(), minimumPoolSize);
	}

	void setPriority(TaskPrioritys priority) {
		::SetThreadpoolCallbackPriority(environment_.get(), priority);
	}

	void setLongRunning() {
		::SetThreadpoolCallbackRunsLong(environment_.get());
	}

protected:
	platform::Environment environment_;

private:
	platform::CleanupGroup cleanupGroup_;
	std::unique_ptr<TP_POOL, decltype(&::CloseThreadpool)> pPool_;
};

class ThreadPool : public BaseThreadPool<platform::WorkableTask> {
public:
	ThreadPool();

	virtual ~ThreadPool();

	virtual void start(std::function<void(void)> action) override;
private:
	mutable bool isShutdown_;
	std::condition_variable weakupEvent_;
	std::thread joinTaskThread_;
	Concurrency::concurrent_queue<TaskPtr> taskes_;
};

}
