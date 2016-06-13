//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <typeinfo>
#include <tuple>
#include <functional>

namespace rapid {

namespace utils {

template <size_t N>
struct Execute {
	template <typename F, typename T, typename...Args>
	static void execute(F &&f, T &&t, Args&&...args) {
		Execute<N - 1>::execute(f, std::forward<T>(t), std::get<N - 1>(std::forward<T>(t)), args...);
	}
};

template <>
struct Execute<0> {
	template <typename F, typename T, typename...Args>
	static void execute(F &&f, T &&t, Args&&...args) {
		f(args...);
	}
};

template <typename Function, typename...Args>
class ScopeGuard {
public:
	static ScopeGuard<Function, Args...> makeScopeGurad(Function func, Args...args) {
		return ScopeGuard(std::move(func), std::move(args)...);
	}

	ScopeGuard(ScopeGuard &&other) {
		*this = std::move(other);
	}

	ScopeGuard& operator=(ScopeGuard &&other) {
		if (this != &other) {
			func = std::move(other.func);
			isDismissed = other.isDismissed;
			other.isDismissed = false;
			para = std::move(para);
		}
		return *this;
	}

	ScopeGuard(Function &&f, Args&&...args)
		: func(f)
		, para(args...)
		, isDismissed(false) {
	}

	ScopeGuard(ScopeGuard const &) = delete;
	ScopeGuard& operator=(ScopeGuard const &) = delete;

	~ScopeGuard() {
		if (!isDismissed) {
			Execute<::std::tuple_size<std::tuple<Args...>>::value>::execute(func, para);
		}
	}

	void dismiss() throw() {
		isDismissed = true;
	}

private:
	Function func;
	std::tuple<Args...> para;
	bool isDismissed;
};

template <typename Function, typename...Args>
auto makeScopeGurad(Function func, Args...args)->ScopeGuard<Function, Args...> {
	return ScopeGuard<Function, Args...>::makeScopeGurad(func, args...);
}

enum class ScopeGuardOnExit {};

template <typename Function>
auto operator+(ScopeGuardOnExit, Function &&func)->ScopeGuard<Function> {
	return makeScopeGurad(func);
}

}

}

#define CONCATENATE_IMPL(name, line) name##line
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#define ANONYMOUS_VARIABLE_NAME(name) CONCATENATE(name, __COUNTER__)

#define SCOPE_EXIT() auto ANONYMOUS_VARIABLE_NAME(ScopeExit) = rapid::utils::ScopeGuardOnExit() + [&]() throw()
