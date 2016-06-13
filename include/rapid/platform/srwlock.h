//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class SRWLock {
public:
	class ReadLockScoped {
	public:
		explicit ReadLockScoped(SRWLock &lock) noexcept
			: lock_(lock) {
			lock_.acquireLockShared();
		}

		ReadLockScoped(ReadLockScoped const &) = delete;
		ReadLockScoped& operator=(ReadLockScoped const &) = delete;

		~ReadLockScoped() noexcept {
			lock_.releaseLockShared();
		}
	private:
		SRWLock &lock_;
	};

	class WriteLockScoped {
	public:
		explicit WriteLockScoped(SRWLock &lock) noexcept
			: lock_(lock) {
			lock_.acquireLockExclusive();
		}

		WriteLockScoped(WriteLockScoped const &) = delete;
		WriteLockScoped& operator=(WriteLockScoped const &) = delete;

		~WriteLockScoped() noexcept {
			lock_.releaseLockExclusive();
		}
	private:
		SRWLock &lock_;
	};

	SRWLock() noexcept;

	~SRWLock() noexcept;

	SRWLock(SRWLock const &) = delete;
	SRWLock& operator=(SRWLock const &) = delete;

private:
	friend class ReadLockScoped;
	friend class WriteLockScoped;

	void acquireLockShared() noexcept;
	void releaseLockShared() noexcept;

	void acquireLockExclusive() noexcept;
	void releaseLockExclusive() noexcept;

	SRWLOCK lock_;
};

__forceinline SRWLock::SRWLock() noexcept {
	::InitializeSRWLock(&lock_);
}

__forceinline SRWLock::~SRWLock() noexcept {
}

__forceinline void SRWLock::acquireLockShared() noexcept {
	::AcquireSRWLockShared(&lock_);
}

__forceinline void SRWLock::releaseLockShared() noexcept {
	::ReleaseSRWLockShared(&lock_);
}

__forceinline void SRWLock::acquireLockExclusive() noexcept {
	::AcquireSRWLockExclusive(&lock_);
}

__forceinline void SRWLock::releaseLockExclusive() noexcept {
	::ReleaseSRWLockExclusive(&lock_);
}

}

}
