//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

#include <rapid/exception.h>
#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

inline HANDLE getImpersonationToken() {
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
		throw Exception();
	}
	return token;
}

inline HANDLE getProcessToken(HANDLE process) {
	HANDLE token = nullptr;
	if (!::OpenProcessToken(process, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token)) {
		throw Exception();
	}
	return token;
}

inline void enablePrivilege(std::wstring const &privilege, HANDLE token, TOKEN_PRIVILEGES* ptpOld) {
	TOKEN_PRIVILEGES tp = { 0 };
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	auto retval = ::LookupPrivilegeValueW(nullptr, privilege.c_str(), &tp.Privileges[0].Luid);
	if (retval) {
		DWORD cbOld = sizeof(TOKEN_PRIVILEGES);
		retval = ::AdjustTokenPrivileges(token, FALSE, &tp, cbOld, ptpOld, &cbOld);
	}
	if (!retval || ::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
		throw Exception();
	}
}

inline void restorePrivilege(HANDLE token, TOKEN_PRIVILEGES* ptpOld) {
	auto retval = ::AdjustTokenPrivileges(token, FALSE, ptpOld, 0, nullptr, nullptr);
	if (!retval || ::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
		throw Exception();
	}
}

}

}
