//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma comment(lib, "DbgHelp.lib")

#include <mutex>
#include <sstream>
#include <filesystem>

#include <rapid/platform/platform.h>

#include <dbghelp.h>

#include <rapid/utils/scopeguard.h>

#include <rapid/platform/privilege.h>
#include <rapid/exception.h>

namespace rapid {

static std::mutex s_miniDumpMutex;

static std::string getErrorMessage(uint32_t lastError) {
    char message[MAX_PATH] = { 0 };

    if (!::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                          nullptr,
                          lastError,
                          MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                          message,
                          MAX_PATH - 1,
                          nullptr)) {
        return (getErrorMessage(::GetLastError()));
    }

    auto lf = strrchr(message, '\r');
    if (lf != nullptr) {
        *lf = '\0';
    }
    return message;
}

// See: http://blogs.msdn.com/oldnewthing/archive/2006/11/03/942851.aspx
static BOOL WIN32_FROM_HRESULT(HRESULT hr, OUT uint32_t *pdwWin32) {
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

static std::wstring makeMiniDumpFileName() {
    wchar_t appPathBuffer[MAX_PATH];
	::GetModuleFileNameW(nullptr, appPathBuffer, MAX_PATH - 1);
    std::tr2::sys::path appPath(appPathBuffer);
    appPath.parent_path().string();
    std::wostringstream ostr;
    ostr << appPath.parent_path().wstring() << L"/" << time(nullptr) << L".dmp";
    return ostr.str();
}

static bool writeMiniDump(std::wstring const &filePath, EXCEPTION_POINTERS *pExceptionInfo) {
	std::lock_guard<std::mutex> guard{ s_miniDumpMutex };

    auto impersonationToken = platform::getImpersonationToken();
    if (!impersonationToken) {
        return false;
    }

	TOKEN_PRIVILEGES tp = { 0 };
	platform::enablePrivilege(SE_DEBUG_NAME, impersonationToken, &tp);
	SCOPE_EXIT() {
		platform::restorePrivilege(impersonationToken, &tp);
    };

	auto miniDumpFileHandle = ::CreateFileW(filePath.c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (miniDumpFileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

	SCOPE_EXIT() {
        ::CloseHandle(miniDumpFileHandle);
    };

    MINIDUMP_EXCEPTION_INFORMATION exInfo = { 0 };
    MINIDUMP_EXCEPTION_INFORMATION *pExtInfo = nullptr;
    if (pExceptionInfo != nullptr) {
        exInfo.ThreadId = ::GetCurrentThreadId();
        exInfo.ExceptionPointers = pExceptionInfo;
        exInfo.ClientPointers = FALSE;
        pExtInfo = &exInfo;
    }

    auto currentProcessHandle = ::GetCurrentProcess();
    uint32_t processId = ::GetProcessId(currentProcessHandle);
    return ::MiniDumpWriteDump(currentProcessHandle,
                               processId,
                               miniDumpFileHandle,
                               MiniDumpNormal,
                               pExtInfo,
                               nullptr,
                               nullptr) != FALSE;
}

uint32_t Exception::fromHRESULT(HRESULT result) {
    uint32_t win32Error;
    if (WIN32_FROM_HRESULT(result, &win32Error)) {
        return win32Error;
    }
    return result;
}

Exception::Exception(char const *message)
	: lastError_(0)
	, message_(message) {
}

Exception::Exception()
	: Exception(::GetLastError()) {
}

Exception::Exception(uint32_t error)
    : lastError_(error) {
    message_ = getErrorMessage(lastError_);
}

uint32_t Exception::error() const {
    return lastError_;
}

Exception::~Exception() {
}

char const * Exception::what() const {
	return message_.c_str();
}

NotFoundException::NotFoundException()
    : Exception(ERROR_FILE_NOT_FOUND) {
}

NotFoundException::~NotFoundException() {
}

ExceptionWithMinidump::ExceptionWithMinidump(char const *expr, char const *file, int lineNumber) {
    miniDumpPath_ = makeMiniDumpFileName();
    writeMiniDump(miniDumpPath_.c_str(), nullptr);
    std::ostringstream ostr;
    ostr << "Failed: " << expr << "\r\n"
         << "File: " << file << " Line: " << lineNumber << "\r\n";
	message_ = ostr.str();
}

ExceptionWithMinidump::~ExceptionWithMinidump() {
}

char const * ExceptionWithMinidump::what() const {
	return message_.c_str();
}

std::wstring ExceptionWithMinidump::getMinidumpPath() const {
    return miniDumpPath_;
}

}
