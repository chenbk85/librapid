//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <chrono>

namespace rapid {

namespace utils {

template <typename Clock>
class Stopwatch final {
public:
    Stopwatch() throw();

    template <typename U>
    typename U::rep elapsedCount() const throw();

    template <typename U>
    typename U elapsed() const throw();

    void reset() throw();
private:
    std::chrono::time_point<Clock> startTimePoint_;
};

template <typename Clock>
inline Stopwatch<Clock>::Stopwatch() throw()
    : startTimePoint_(Clock::now()) {
}

template <typename Clock>
template <typename U>
inline typename U::rep Stopwatch<Clock>::elapsedCount() const throw() {
    return elapsed<U>().count();
}

template <typename Clock>
template <typename U>
inline typename U Stopwatch<Clock>::elapsed() const throw() {
    return std::chrono::duration_cast<U>(Clock::now() - startTimePoint_);
}

template <typename Clock>
inline void Stopwatch<Clock>::reset() throw() {
    startTimePoint_ = Clock::now();
}

typedef Stopwatch<std::chrono::system_clock> SystemStopwatch;
typedef Stopwatch<std::chrono::high_resolution_clock> HighResolutionStopwatch;

}

}



