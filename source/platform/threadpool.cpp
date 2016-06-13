//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/platform/threadpool.h>

namespace rapid {

namespace platform {

Environment::Environment() throw() {
	::InitializeThreadpoolEnvironment(&environ_);
}

Environment::~Environment() throw() {
	::DestroyThreadpoolEnvironment(&environ_);
}

CallbackInstance::CallbackInstance()
	: instance_(nullptr) {
}

bool CallbackInstance::isMayLongRunning() const throw() {
	return ::CallbackMayRunLong(instance_) != FALSE;
}

void CallbackInstance::disassociate() throw() {
	::DisassociateCurrentThreadFromCallback(instance_);
}

void CallbackInstance::setEventWhenTaskReturns(HANDLE event) throw() {
	::SetEventWhenCallbackReturns(instance_, event);
}

CleanupGroup::CleanupGroup() throw() {
	pCleanupGroup_ = ::CreateThreadpoolCleanupGroup();
}

CleanupGroup::~CleanupGroup() throw() {
	// Cancel pending callbacks and waits for the ones currently executing.
	if (pCleanupGroup_ != nullptr) {
		::CloseThreadpoolCleanupGroupMembers(pCleanupGroup_, TRUE, nullptr);
		::CloseThreadpoolCleanupGroup(pCleanupGroup_);
	}
}

WaitableTask::WaitableTask(std::function<void(void)> action_, Environment &env)
	: action_(action_) {
	pWait_ = ::CreateThreadpoolWait(callback, this, env.get());
	RAPID_ENSURE(pWait_ != nullptr);
}

WaitableTask::~WaitableTask() throw() {
	if (pWait_ != nullptr) {
		::CloseThreadpoolWait(pWait_);
	}
}

void WaitableTask::run(HANDLE waitForObject, uint32_t timeoutAsMs) {
	timeoutAsMs_ = timeoutAsMs;
	waitForObject_ = waitForObject;
	::SetThreadpoolWait(pWait_, waitForObject_, createTimeout());
}

void WaitableTask::cancel() {
	::SetThreadpoolWait(pWait_, nullptr, nullptr);
	waitForObject_ = nullptr;
	timeoutAsMs_ = 0;
}

FILETIME * WaitableTask::createTimeout() throw() {
	if (timeoutAsMs_ == INFINITE) {
		return nullptr;
	}

	LARGE_INTEGER const largeInteger = {
		timeoutAsMs_ * 10000000
	};
	waitFiletime_.dwHighDateTime = largeInteger.HighPart;
	waitFiletime_.dwLowDateTime = largeInteger.LowPart;
	return &waitFiletime_;
}

void CALLBACK WaitableTask::callback(PTP_CALLBACK_INSTANCE instance,
	PVOID param,
	PTP_WAIT wait,
	TP_WAIT_RESULT waitResult) {
	auto pWaitableItem = reinterpret_cast<WaitableTask*>(param);
	pWaitableItem->instance_ = instance;
	pWaitableItem->action_();
	::SetThreadpoolWait(pWaitableItem->pWait_, pWaitableItem->waitForObject_, pWaitableItem->createTimeout());
}

WorkableTask::WorkableTask(std::function<void(void)> action, Environment &environment)
	: action_(action) {
	pWork_ = ::CreateThreadpoolWork(callback, this, environment.get());
	RAPID_ENSURE(pWork_ != nullptr);
}

WorkableTask::WorkableTask(Environment &environment) {
	pWork_ = ::CreateThreadpoolWork(callback, this, environment.get());
	RAPID_ENSURE(pWork_ != nullptr);
}

WorkableTask::~WorkableTask() throw() {
	if (pWork_ != nullptr) {
		::CloseThreadpoolWork(pWork_);
	}
}

void WorkableTask::run(std::function<void(void)> action) {
	RAPID_ENSURE(pWork_ != nullptr);
	action_ = action;
	run();
}

void WorkableTask::run() const {
	RAPID_ENSURE(pWork_ != nullptr);
	::SubmitThreadpoolWork(pWork_);
}

void WorkableTask::join() const {
	RAPID_ENSURE(pWork_ != nullptr);
	::WaitForThreadpoolWorkCallbacks(pWork_, FALSE);
}

void WorkableTask::cancel() const {
	RAPID_ENSURE(pWork_ != nullptr);
	::WaitForThreadpoolWorkCallbacks(pWork_, TRUE);
}

void CALLBACK WorkableTask::callback(PTP_CALLBACK_INSTANCE instance, PVOID ctx, PTP_WORK work) {
	auto pTask = reinterpret_cast<WorkableTask*>(ctx);
	pTask->instance_ = instance;

	try {
		pTask->action_();
	} catch (...) {
	}
}

}

ThreadPool::ThreadPool()
	: isShutdown_(false) {
	setPoolSize(2000, 2000);
	joinTaskThread_ = std::thread([this] {
		std::mutex mutex;
		std::unique_lock<std::mutex> lock{ mutex };

		while (true) {
			while (taskes_.empty()) {
				weakupEvent_.wait(lock);
			}

			if (isShutdown_) {
				return;
			}

			TaskPtr task;
			if (taskes_.try_pop(task)) {
				task->join();
			}
		}
	});
}

ThreadPool::~ThreadPool() {
	isShutdown_ = true;
	weakupEvent_.notify_one();
	if (joinTaskThread_.joinable()) {
		joinTaskThread_.join();
	}
}

void ThreadPool::start(std::function<void(void)> action) {
	auto pTask = std::make_shared<platform::WorkableTask>(action, environment_);
	pTask->run();
	taskes_.push(std::move(pTask));
	weakupEvent_.notify_one();
}

}
