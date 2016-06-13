//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015 IOCP Framework project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Win32Exception : public std::exception {
public:
    explicit Win32Exception(DWORD error);

    virtual ~Win32Exception();

    virtual char const * what() const override;

    DWORD error() const;

protected:
    std::string message;
    DWORD lastError;
};

class NotFoundException : public Win32Exception {
public:
    explicit NotFoundException(DWORD error = ERROR_FILE_NOT_FOUND);

    virtual ~NotFoundException();
};

class ExceptionWithMinidump : public std::exception {
public:
    ExceptionWithMinidump(char const *expr, char const *file, int lineNumber);

    virtual ~ExceptionWithMinidump();

    virtual char const * what() const override;

    std::wstring getMinidumpPath() const;
private:
    std::wstring miniDumpPath;
    std::string message;
};

