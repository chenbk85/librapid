//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

#include <rapid/platform/platform.h>

#include <rapid/logging/logging.h>

namespace rapid {

namespace logging {

class EventLogAppender : public LogAppender {
public:
    explicit EventLogAppender(std::wstring const &serverName);

    virtual ~EventLogAppender();

	virtual void write(LogFormatter &format, const LogEntry &record) override;

private:
    void reportEvent(WORD eventType, std::string const &msg);
	std::wstring reportServerName_;
    HANDLE handle_;
};

}

}
