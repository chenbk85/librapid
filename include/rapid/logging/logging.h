//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <sstream>
#include <thread>
#include <memory>

#include <sys/types.h>
#include <sys/timeb.h>

namespace rapid {

namespace logging {

extern char const * LogLevelNameTable[];

enum Level : uint8_t {
    Fatal = 0,
    Error,
    Warn,
    Info,
    Trace,
    _MAX_LEVEL_,
};

struct LogEntry {
	LogEntry()
		: file(nullptr)
		, line(0)
		, level(_MAX_LEVEL_) {
	}

	char const *file;
	int line;
	Level level;
	std::thread::id threadId;
	std::string message;
	__timeb64 timestamp;
};

class LogFormatter : public std::enable_shared_from_this<LogFormatter> {
public:
	virtual ~LogFormatter() = default;

	LogFormatter(LogFormatter const &) = delete;
	LogFormatter& operator=(LogFormatter const &) = delete;

	virtual std::ostream& format(std::ostream &ostr, LogEntry const &record) = 0;

protected:
	LogFormatter() = default;
};

class LogAppender : public std::enable_shared_from_this<LogAppender> {
public:
	virtual ~LogAppender() = default;

	LogAppender(LogAppender const &) = delete;
	LogAppender& operator=(LogAppender const &) = delete;

	virtual void write(LogFormatter &format, const LogEntry &entry) = 0;

protected:
	friend std::ostream& operator<<(std::ostream &ostr, __timeb64 const &timestamp);

	LogAppender() = default;
};

Level getLogLevel();

void setLogLevel(Level level);

void startLogging(Level level);

void swapLogAppender(std::shared_ptr<LogAppender> pAppender);

void addLogAppender(std::shared_ptr<LogAppender> pAppender);

void stopLogging();

class LogEntryWrapper {
public:   
	LogEntryWrapper(char const *file, int line, char const *function, Level level);

    LogEntryWrapper(LogEntryWrapper const &) = delete;
    LogEntryWrapper& operator=(LogEntryWrapper const &) = delete;

    ~LogEntryWrapper();

	__forceinline std::ostringstream& stream() noexcept {
		return stream_;
	}

private:
	std::ostringstream stream_;
    LogEntry entry_;
};

}

}

#define RAPID_LOG(Level) \
    if (Level <= rapid::logging::getLogLevel()) \
        rapid::logging::LogEntryWrapper::LogEntryWrapper(__FILE__, __LINE__, __FUNCTION__, Level).stream()

#define RAPID_LOG_FATAL() RAPID_LOG(rapid::logging::Fatal)
#define RAPID_LOG_ERROR() RAPID_LOG(rapid::logging::Error)
#define RAPID_LOG_WARN()  RAPID_LOG(rapid::logging::Warn)
#define RAPID_LOG_INFO()  RAPID_LOG(rapid::logging::Info)
#define RAPID_LOG_TRACE() RAPID_LOG(rapid::logging::Trace)

#define RAPID_LOG_FUNC_NAME() __FUNCTION__##"() "

#define RAPID_LOG_TRACE_INFO() RAPID_LOG_INFO() << RAPID_LOG_FUNC_NAME()
#define RAPID_LOG_TRACE_FUNC() RAPID_LOG_TRACE() << RAPID_LOG_FUNC_NAME()
#define RAPID_LOG_ERROR_FUNC() RAPID_LOG_ERROR() << RAPID_LOG_FUNC_NAME()
#define RAPID_LOG_WARN_FUNC()  RAPID_LOG_WARN()  << RAPID_LOG_FUNC_NAME()
#define RAPID_LOG_FATAL_FUNC() RAPID_LOG_FATAL() << RAPID_LOG_FUNC_NAME()
