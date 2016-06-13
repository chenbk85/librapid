//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#ifdef RAPID_USE_STDAFX_FILE
#include "stdafx.h"
#endif

#include <mutex>
#include <sstream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <dbghelp.h>

#include <rapid/base/scopeguard.h>

#include <rapid/platform/win32exception.h>

static std::string getErrorMessage(DWORD lastError) {
    char message[MAX_PATH];

    if (!::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                          0,
                          lastError,
                          MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                          message,
                          MAX_PATH - 1,
                          nullptr)) {
        return (getErrorMessage(::GetLastError()));
    }

    return message;
}

// See: http://blogs.msdn.com/oldnewthing/archive/2006/11/03/942851.aspx
static BOOL WIN32_FROM_HRESULT(HRESULT hr, OUT DWORD *pdwWin32) {
    if ((hr & 0xFFFF0000) == MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0)) {
        // Could have come from many values, but we choose this one
        *pdwWin32 = HRESULT_CODE(hr);
        return TRUE;
    }
    if (hr == S_OK) {
        *pdwWin32 = HRESULT_CODE(hr);
        return TRUE;
    }
    // otherwise, we got an impossible value
    return FALSE;
}

static HANDLE getImpersonationToken() {
    HANDLE token;

    auto retval = ::OpenThreadToken(::GetCurrentThread(),
                                    TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                                    TRUE,
                                    &token);

    if (!retval && ::GetLastError() == ERROR_NO_TOKEN) {
        retval = ::OpenProcessToken(::GetCurrentProcess(),
                                    TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                                    &token);
    }

    if (!retval) {
        return nullptr;
    }

    return token;
}

static bool restorePrivilege(HANDLE token, TOKEN_PRIVILEGES* ptpOld) {
    auto retval = ::AdjustTokenPrivileges(token, FALSE, ptpOld, 0, nullptr, nullptr);
    return (retval && (ERROR_NOT_ALL_ASSIGNED != ::GetLastError()));
}

static bool enablePrivilege(std::wstring const &privilege, HANDLE token, TOKEN_PRIVILEGES* ptpOld) {
    TOKEN_PRIVILEGES tp;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    auto retval = ::LookupPrivilegeValueW(0, privilege.c_str(), &tp.Privileges[0].Luid);
    return (retval && (ERROR_NOT_ALL_ASSIGNED != ::GetLastError()));
}

static std::wstring makeMiniDumpFileName() {
    wchar_t appPathBuffer[_MAX_PATH];
    ::GetModuleFileNameW(nullptr, appPathBuffer, _MAX_PATH - 1);

    std::tr2::sys::wpath appPath(appPathBuffer);
    appPath.parent_path().string();

    std::wostringstream ostr;
    ostr << appPath.parent_path().string() << L"/" << time(nullptr) << L".dmp";
    return ostr.str();
}

static bool writeMiniDump(std::wstring const &filePath, EXCEPTION_POINTERS *pExceptionInfo) {
    static std::mutex miniDumpMutex;;

    std::lock_guard<std::mutex> guard(miniDumpMutex);

    auto impersonationToken = getImpersonationToken();
    if (!impersonationToken) {
        return false;
    }

    TOKEN_PRIVILEGES tp;

    bool isEnablePrivilegeSuccess = enablePrivilege(L"SeDebugPrivilege", impersonationToken, &tp);

    auto resotrePrivilege = makeScopeGurad([&] {
        if (isEnablePrivilegeSuccess) {
            restorePrivilege(impersonationToken, &tp);
        }
    });

    HANDLE miniDumpFileHandle = ::CreateFileW(filePath.c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);

    if (miniDumpFileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    auto autoCloseFile = makeScopeGurad([&] {
        ::CloseHandle(miniDumpFileHandle);
    });

    MINIDUMP_EXCEPTION_INFORMATION exInfo = { 0 };
    MINIDUMP_EXCEPTION_INFORMATION *pExtInfo = nullptr;

    if (pExceptionInfo != nullptr) {
        exInfo.ThreadId = ::GetCurrentThreadId();
        exInfo.ExceptionPointers = pExceptionInfo;
        exInfo.ClientPointers = FALSE;
        pExtInfo = &exInfo;
    }

    auto currentProcessHandle = ::GetCurrentProcess();
    DWORD processId = ::GetProcessId(currentProcessHandle);

    return ::MiniDumpWriteDump(currentProcessHandle,
                               processId,
                               miniDumpFileHandle,
                               MiniDumpNormal,
                               pExtInfo,
                               nullptr,
                               nullptr) != FALSE;
}

DWORD Win32Exception::fromHRESULT(HRESULT result) {
    DWORD win32Error;
    if (WIN32_FROM_HRESULT(result, &win32Error)) {
        return win32Error;
    }
    return result;
}

Win32Exception::Win32Exception(DWORD error)
    : lastError(error) {
    message = getErrorMessage(lastError);
}

DWORD Win32Exception::error() const {
    return lastError;
}

Win32Exception::~Win32Exception() {

}

char const * Win32Exception::what() const {
    return message.c_str();
}

NotFoundException::NotFoundException(DWORD error)
    : Win32Exception(error) {
}

NotFoundException::~NotFoundException() {

}

ExceptionWithMinidump::ExceptionWithMinidump(char const *expr, char const *file, int lineNumber) {
    miniDumpPath = makeMiniDumpFileName();
    writeMiniDump(miniDumpPath.c_str(), nullptr);

    std::ostringstream ostr;
    ostr << "Failed: " << expr << "\r\n"
         << "File: " << file << " Line: " << lineNumber << "\r\n";
    message = ostr.str();
}

ExceptionWithMinidump::~ExceptionWithMinidump() {

}

char const * ExceptionWithMinidump::what() const {
    return message.c_str();
}

std::wstring ExceptionWithMinidump::getMinidumpPath() const {
    return miniDumpPath;
}
