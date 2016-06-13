//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <ctime>
#include <string>
#include <sstream>
#include <atomic>
#include <thread>
#include <iomanip>
#include <filesystem>
#include <mutex>
#include <condition_variable>
#include <concurrent_queue.h>

#include <rapid/details/contracts.h>

#include <rapid/utils/singleton.h>
#include <rapid/utils/stringutilis.h>

#include <rapid/logging/timestamp.h>
#include <rapid/logging/stackdump.h>
#include <rapid/logging/logging.h>

namespace rapid {

namespace logging {

std::string shorestFileName(std::string const &fileName) {
    auto const pos = fileName.find_last_of("(/\\");
    return fileName.substr(pos + 1);
}

char const * LogLevelNameTable[_MAX_LEVEL_] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "TRACE",
};

static std::atomic<Level> s_logLevel{ Level::Fatal };
static std::atomic<PVOID> s_vectoredExceptionHandle{ nullptr };
static std::atomic<uint32_t> s_countOfEnterExceptionHandler{ 0 };

// 如果要Stackdump讀取正確的函數名稱需要pdb檔案配合才行!
static LONG logFatalStackTrace(EXCEPTION_POINTERS const *exceptionPtr, char const *handler) {
#ifdef _DEBUG
	__debugbreak();
#endif
	if (s_countOfEnterExceptionHandler > 0) {
		// 程式正常結束中時候又發生Fatal exception!
		// TODO: 強制結束程式?
		return EXCEPTION_CONTINUE_SEARCH;
	}

	++s_countOfEnterExceptionHandler;

	RAPID_LOG_FATAL() << "Received fatal signal "
		<< details::exceptionCodeToString(exceptionPtr->ExceptionRecord->ExceptionCode)
		<< "(0x" << std::hex << exceptionPtr->ExceptionRecord->ExceptionCode << ")\t"
		<< "PID: " << std::dec << ::GetProcessId(nullptr)
		<< "\r\n"
		<< details::getStackTrace(exceptionPtr);

	stopLogging();

	// 正常結束程式, 解構所有的物件
	std::exit(EXIT_FAILURE);
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI vectorExceptionHandling(EXCEPTION_POINTERS *exceptionPtr) {
    if (!details::isKnownExceptionCode(exceptionPtr->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    return logFatalStackTrace(exceptionPtr, "Vectored Exception Handler");
}

class LoggingWorker final {
public:
    LoggingWorker();

    LoggingWorker(LoggingWorker const &) = delete;
    LoggingWorker& operator=(LoggingWorker const &) = delete;

    ~LoggingWorker();

    template <typename EventHandler>
	void add(EventHandler &&writeLogCallback) {
		actions_.push(std::move(writeLogCallback));
		cond_.notify_one();
    }

    void stop();
private:
	volatile bool stopped_ : 1;
	Concurrency::concurrent_queue<std::function<void()>> actions_;
    std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

LoggingWorker::LoggingWorker()
	: stopped_(false) {
	thread_ = std::thread([this] {
		while (!stopped_) {
			while (actions_.empty()) {
				std::unique_lock<std::mutex> lock{ mutex_ };
				cond_.wait(lock);
			}

			std::function<void()> action;
			while (actions_.try_pop(action)) {
				action();
			}
        }
    });
}

void LoggingWorker::stop() {
	stopped_ = true;
    add([] {}); // Weakup theaed to stop!
	if (thread_.joinable()) {
		thread_.join();
    }
}

LoggingWorker::~LoggingWorker() {
    stop();
}

class DefaultLogFormatter final : public LogFormatter {
public:
    DefaultLogFormatter() = default;

    virtual ~DefaultLogFormatter() = default;

	virtual std::ostream& format(std::ostream &ostr, LogEntry const &entry) override;
};

std::ostream& operator << (std::ostream &ostr, __timeb64 const &timestamp) {
	tm localTime = { 0 };

	::_localtime64_s(&localTime, &timestamp.time);

	ostr << std::setiosflags(std::ios::right) 
		<< std::setfill('0')
		<< details::putTime(&localTime, "%d %b %H:%M:%S")
		<< "."
		<< std::setw(3)
		<< timestamp.millitm;
	return ostr;
}

std::ostream& DefaultLogFormatter::format(std::ostream &ostr, LogEntry const &entry) {
	ostr << "["
		<< std::setw(5)
		<< std::setfill(' ')
		<< entry.threadId
		<< "] "
		<< entry.timestamp
		<< std::setfill(' ')
		<< std::setw(5) << LogLevelNameTable[entry.level]
		<< " * "
		<< entry.message;
    return ostr;
}

class Logger final : public utils::Singleton<Logger> {
public:
    Logger();

    Logger(Logger const &) = delete;
    Logger& operator=(Logger const &) = delete;

    void append(LogEntry entry);

    void swapAppender(std::shared_ptr<LogAppender> pSink);

	void removeAppender();

    void addAppender(std::shared_ptr<LogAppender> pSink);

    void stop();

private:
    LoggingWorker worker_;
    std::vector<std::shared_ptr<LogAppender>> appenders_;
    std::shared_ptr<LogFormatter> pFormatter_;
};

Logger::Logger()
	: pFormatter_(std::make_shared<DefaultLogFormatter>()) {
	s_vectoredExceptionHandle = ::AddVectoredExceptionHandler(0, vectorExceptionHandling);
}

void Logger::stop() {
	worker_.stop();
	if (s_vectoredExceptionHandle != nullptr) {
		::RemoveVectoredContinueHandler(s_vectoredExceptionHandle);
	}
}

void Logger::append(LogEntry entry) {
	worker_.add([this, entry]() {
		for (auto & pAppender : appenders_) {
			pAppender->write(*pFormatter_, entry);
        }
    });
}

void Logger::swapAppender(std::shared_ptr<LogAppender> pAppender) {
    auto pNewAppender = pAppender->shared_from_this();
	worker_.add([this, pNewAppender] {
		if (!appenders_.empty()) {
			return;
		}
		appenders_[0].reset();
		appenders_[0] = pNewAppender;
    });
}

void Logger::removeAppender() {
	worker_.add([this] {
		appenders_.pop_back();
	});
}

void Logger::addAppender(std::shared_ptr<LogAppender> pAppender) {
	auto pNewAppender = pAppender->shared_from_this();
	worker_.add([this, pNewAppender] {
		appenders_.push_back(pNewAppender);
	});
}

LogEntryWrapper::LogEntryWrapper(char const *file, int line, char const *function, logging::Level level) {
	RAPID_ENSURE(level <= _MAX_LEVEL_);
	entry_.level = level;
    entry_.threadId = std::this_thread::get_id();
    ::_ftime64_s(&entry_.timestamp);
    entry_.file = file;
    entry_.line = line;
}

LogEntryWrapper::~LogEntryWrapper() {
	entry_.message = stream_.str();

	if (s_vectoredExceptionHandle != nullptr) {
		Logger::getInstance().append(entry_);
	}
}

void startLogging(Level level) {
	s_logLevel = level;
	RAPID_ENSURE(s_vectoredExceptionHandle == nullptr);
	Logger::getInstance();
}

void setLogLevel(Level level) {
	s_logLevel = level;
}

Level getLogLevel() {
	return s_logLevel;
}

void removeLogAppender() {
	if (s_vectoredExceptionHandle != nullptr) {
		Logger::getInstance().removeAppender();
	}
}

void swapLogAppender(std::shared_ptr<LogAppender> pAppender) {
	if (s_vectoredExceptionHandle != nullptr) {
		Logger::getInstance().swapAppender(pAppender);
    }
}

void addLogAppender(std::shared_ptr<LogAppender> pAppender) {
	if (s_vectoredExceptionHandle != nullptr) {
		Logger::getInstance().addAppender(pAppender);
    }
}

void stopLogging() {
	if (s_vectoredExceptionHandle != nullptr) {
		Logger::getInstance().stop();
		s_vectoredExceptionHandle = nullptr;
    }
}

}

}
