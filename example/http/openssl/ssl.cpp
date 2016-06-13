//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>

#include <windows.h>
#include <wincrypt.h>

#include "openssl.h"
#include "ssl.h"

extern "C" {
	struct CRYPTO_dynlock_value {
		std::mutex mutex;
	};
}

static std::mutex *s_sslCryptoLocks;

static void crtptoLockCallback(int mode, int n, const char* /* file */, int /* line */) {
	if (mode & CRYPTO_LOCK) {
		s_sslCryptoLocks[n].lock();
	} else {
		s_sslCryptoLocks[n].unlock();
	}
}

static unsigned long crtptoIdCallbakc() {
	return static_cast<uint32_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
}

struct CRYPTO_dynlock_value* dynlockCreateCallback(const char* /* file */, int /* line */) {
	return new CRYPTO_dynlock_value;
}

void dynlockCallback(int mode, struct CRYPTO_dynlock_value* lock, const char* /* file */, int /* line */) {
	if (mode & CRYPTO_LOCK)
		lock->mutex.lock();
	else
		lock->mutex.unlock();
}

void dynlockDestroyCallback(struct CRYPTO_dynlock_value* lock, const char* /* file */, int /* line */) {
	delete lock;
}

static void initializeSSL() {
	::SSL_library_init();
	::SSL_load_error_strings();

	OpenSSL_add_all_algorithms();

	char seed[256] = { 0 };
	getSeed(seed, sizeof(seed));
	::RAND_seed(seed, sizeof(seed));

	s_sslCryptoLocks = new std::mutex[::CRYPTO_num_locks()];

	::CRYPTO_set_locking_callback(crtptoLockCallback);
	::CRYPTO_set_id_callback(crtptoIdCallbakc);
	::CRYPTO_set_dynlock_create_callback(dynlockCreateCallback);
	::CRYPTO_set_dynlock_lock_callback(dynlockCallback);
	::CRYPTO_set_dynlock_destroy_callback(dynlockDestroyCallback);
}

static void uninitializeSSL() {
	::EVP_cleanup();
	::ERR_free_strings();
	::CRYPTO_set_locking_callback(nullptr);
	delete[] s_sslCryptoLocks;
}

SSLInitializer::SSLInitializer() {
	initializeSSL();
}

SSLInitializer::~SSLInitializer() {
	uninitializeSSL();
}
