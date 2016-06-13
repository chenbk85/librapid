//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <ctime>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

#include <sys/timeb.h>

namespace rapid {

namespace logging {

namespace details {

inline std::time_t currentTime() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

inline std::string putTime(std::tm const* pTm, char const * format) {
    std::ostringstream ostr;
    ostr.fill('0');
    ostr << std::put_time(const_cast<struct tm*>(pTm), format);
    return ostr.str();
}

inline tm localTime(std::time_t const & time) {
    struct tm tmValue;
    ::localtime_s(&tmValue, &time);
    return tmValue;
}

inline std::string printTime(std::time_t const & time, std::string const & format) {
    auto localtime = localTime(time);
    return putTime(&localtime, format.c_str());
}

}

}

}
