//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/details/socketexception.h>

namespace rapid {

namespace details {

SocketException::SocketException(std::string const &msg, std::string const &detail) noexcept
    : Exception() {
    message_.append(": ");
    message_.append(msg);
    message_.append(" " + detail);
}

SocketException::~SocketException() noexcept {
}

const char *SocketException::what() const noexcept {
	return message_.c_str();
}

}

}
