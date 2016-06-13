//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <sstream>

#include <rapid/exception.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/registry.h>
#include <rapid/logging/eventlog.h>

namespace rapid {

namespace logging {

static void installEventLogSource(std::wstring const &appPath, std::wstring const &appName) {
	std::wostringstream ostr;
	ostr << L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" << appName;
	
	auto branch = platform::Registry::createLocalMachineBranch(ostr.str());
	
	// Register EventMessageFile
	branch->setString(L"EventMessageFile", appPath);
	
	// Register supported event types
	DWORD supportLoggingTypes =
		EVENTLOG_ERROR_TYPE |
		EVENTLOG_WARNING_TYPE |
		EVENTLOG_INFORMATION_TYPE;

	branch->setDword(L"TypesSupported", supportLoggingTypes);
}

static void uninstallEventLogSource(std::wstring const &appName) {
	std::wostringstream ostr;
	ostr << L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" << appName;
	platform::Registry::deleteLocalMachineBranch(ostr.str());
}

EventLogAppender::EventLogAppender(std::wstring const &serverName)
	: reportServerName_(serverName)
	, handle_(nullptr) {
	uninstallEventLogSource(serverName);
	installEventLogSource(platform::getApplicationFilePath(), reportServerName_);
	handle_ = ::RegisterEventSourceW(nullptr, reportServerName_.c_str());
	if (!handle_) {
        throw Exception(::GetLastError());
    }
}

EventLogAppender::~EventLogAppender() {
	if (handle_) {
		::DeregisterEventSource(handle_);
    }
	if (!reportServerName_.empty()) {
		uninstallEventLogSource(reportServerName_);
	}
}

void EventLogAppender::write(LogFormatter &format, const LogEntry &record) {
    WORD eventType = 0;

	switch (record.level) {
    case Error:
        eventType = EVENTLOG_ERROR_TYPE;
        break;
    case Warn:
        eventType = EVENTLOG_WARNING_TYPE;
        break;
    case Fatal:
    case Info:
    case Trace:
        eventType = EVENTLOG_INFORMATION_TYPE;
        break;
    default:
        return;
    }

	std::ostringstream ostr;
	format.format(ostr, record);
	reportEvent(eventType, ostr.str());
}

void EventLogAppender::reportEvent(WORD eventType, std::string const &msg) {
    char const *buf[1] = {
        msg.c_str(),
    };
	if (!::ReportEventA(handle_, eventType, 0, 1, nullptr, 1, 0, static_cast<LPCSTR*>(buf), nullptr)) {

    }
}

}

}
