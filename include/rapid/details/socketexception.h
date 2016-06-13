//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <string>

#include <rapid/exception.h>

namespace rapid {

namespace details {

class SocketException : public Exception {
public:
	explicit SocketException(std::string const &msg, std::string const &detail = "") noexcept;

    virtual ~SocketException() noexcept;

	virtual const char *what() const noexcept override;
};

}

}
