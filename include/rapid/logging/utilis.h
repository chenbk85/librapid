//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <chrono>

#include <rapid/platform/platform.h>
#include <rapid/logging/logging.h>

namespace rapid {

namespace logging {

class ConsoleOutputLogAppender final : public LogAppender {
public:
	ConsoleOutputLogAppender();

	virtual ~ConsoleOutputLogAppender() override;

	void setConsoleFont(std::wstring faceName, uint16_t fontSize) const;

	void setWindowSize(size_t width, size_t height);

	virtual void write(LogFormatter &format, LogEntry const &record) override;
private:
	HANDLE stdout_;
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo_;
};

class DebugOutputLogSink final : public LogAppender {
public:
    virtual ~DebugOutputLogSink() override;

	virtual void write(LogFormatter &format, LogEntry const &record) override;
};

class FileLogAppender final : public LogAppender {
public:
	FileLogAppender();

	virtual ~FileLogAppender() = default;

	void setLogDirectory(std::wstring const &directory);

	void setLogFileName(std::wstring const &logFileName);

	void setLimitLogSize(int size);

	virtual void write(LogFormatter &format, LogEntry const &entry) override;

private:
	static uint32_t constexpr LIMIT_FILE_LOG_SIZE = 128 * 1024;

	void createLogFile();

	void compressionDayBeforeLogFile();

	void truncate(size_t writtenSize);

	std::chrono::system_clock::time_point logStartDate_;
	std::ofstream logging_;
	std::wstring logFilePath_;
	std::wstring logFileName_;
	uint32_t limitLogSize_;
};

}

}
