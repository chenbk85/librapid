//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <mutex>

#include <rapid/platform/platform.h>

#include <rapid/details/contracts.h>

namespace rapid {

namespace platform {

// Note: EnterCriticalSection allows for recursive calls from the same thread.
class Spinlock {
public:
    Spinlock();

    Spinlock(Spinlock const &) = delete;
    Spinlock& operator=(Spinlock const &) = delete;

    ~Spinlock() noexcept;

    void lock() const noexcept;

    void unlock() const noexcept;

    bool try_lock() const noexcept;

private:
	// 0x80000000 可以建立一個事件核心物件, 如果不能建立一個事件核心物件時, InitializeCriticalSectionAndSpinCount就會返回FALSE.
	static DWORD constexpr SPIN_COUNT = (0x80000000 | 4000);
    std::unique_ptr<CRITICAL_SECTION> pCS_;
};

template <typename TryLockable>
std::unique_lock<TryLockable> tryToLock(TryLockable& lockable) {
	return std::unique_lock<TryLockable>(lockable, std::try_to_lock);
}

__forceinline Spinlock::Spinlock()
    : pCS_(std::make_unique<CRITICAL_SECTION>()) {
	RAPID_ENSURE(::InitializeCriticalSectionAndSpinCount(pCS_.get(), SPIN_COUNT) == TRUE);
}

__forceinline Spinlock::~Spinlock() noexcept {
    ::DeleteCriticalSection(pCS_.get());
}

__forceinline void Spinlock::lock() const noexcept {
    ::EnterCriticalSection(pCS_.get());
}

__forceinline void Spinlock::unlock() const noexcept {
    ::LeaveCriticalSection(pCS_.get());
}

__forceinline bool Spinlock::try_lock() const noexcept {
    return ::TryEnterCriticalSection(pCS_.get()) != FALSE;
}

}

}
