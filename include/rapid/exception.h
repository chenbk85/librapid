//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

#include <rapid/platform/platform.h>

namespace rapid {

class Exception : public std::exception {
public:
	Exception();

	Exception(char const *message);

    explicit Exception(uint32_t error);

    virtual ~Exception();

    virtual char const * what() const override;

    uint32_t error() const;

    static uint32_t fromHRESULT(HRESULT result);

protected:
    std::string message_;
    uint32_t lastError_;
};

class NotFoundException : public Exception {
public:
    NotFoundException();

    virtual ~NotFoundException();
};

class ExceptionWithMinidump : public std::exception {
public:
    ExceptionWithMinidump(char const *expr, char const *file, int lineNumber);

    virtual ~ExceptionWithMinidump();

    virtual char const * what() const override;

    std::wstring getMinidumpPath() const;
private:
    std::wstring miniDumpPath_;
    std::string message_;
};

}
