//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#if 1

namespace rapid {

namespace utils {

// Double-Checked Locking is Fixed In C++11
// http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
template <typename T>
class Singleton {
public:
	Singleton(Singleton<T> const &) = delete;
	Singleton& operator=(Singleton<T> const &) = delete;

	static T& getInstance();

protected:
	Singleton() = default;
	~Singleton() = default;
};

template <typename T>
inline T& Singleton<T>::getInstance() {
	static T inst;
	return inst;
}

#else

#include <mutex>
#include <atomic>
#include <memory>

template <typename T>
class Singleton final {
public:
	Singleton(Singleton<T> const &) = delete;
	Singleton& operator=(Singleton<T> const &) = delete;

	Singleton() = delete;
	~Singleton() = delete;

	static T& getInstance();

private:
	static std::unique_ptr<T> pInstance;
	static std::once_flag onceFlag;
};

template <typename T>
std::unique_ptr<T> Singleton<T>::pInstance = nullptr;

template <typename T>
std::once_flag Singleton<T>::onceFlag;

template <typename T>
inline T& Singleton<T>::getInstance() {
	std::call_once(onceFlag, [&] {
		pInstance.reset(new T());
	});
	return *pInstance;
}

#endif

}

}