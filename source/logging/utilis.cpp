//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <iostream>
#include <filesystem>

#include <date.h>
#include <libzippp.h>

#include <rapid/platform/memorymappedfile.h>
#include <rapid/utils/stringutilis.h>
#include <rapid/exception.h>

#include <rapid/logging/timestamp.h>
#include <rapid/logging/utilis.h>

namespace rapid {

namespace logging {

enum ConsoleTextColor : WORD {
	BLACK = 0,
	DARKBLUE = FOREGROUND_BLUE,
	DARKGREEN = FOREGROUND_GREEN,
	DARKCYAN = FOREGROUND_GREEN | FOREGROUND_BLUE,
	DARKRED = FOREGROUND_RED,
	DARKMAGENTA = FOREGROUND_RED | FOREGROUND_BLUE,
	DARKYELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
	DARKGRAY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
	GRAY = FOREGROUND_INTENSITY,
	BLUE = FOREGROUND_INTENSITY | FOREGROUND_BLUE,
	GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
	CYAN = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
	RED = FOREGROUND_INTENSITY | FOREGROUND_RED,
	MAGENTA = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
	YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
	WHITE = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};

static inline std::ostream& operator<<(std::ostream& s, ConsoleTextColor color) {
	::SetConsoleTextAttribute(::GetStdHandle(STD_OUTPUT_HANDLE), color);
	return s;
}

ConsoleOutputLogAppender::ConsoleOutputLogAppender()
    : LogAppender()
    , stdout_(::GetStdHandle(STD_OUTPUT_HANDLE)) {
    ::GetConsoleScreenBufferInfo(stdout_, &bufferInfo_);
}

ConsoleOutputLogAppender::~ConsoleOutputLogAppender() {
    ::SetConsoleTextAttribute(stdout_, bufferInfo_.wAttributes);
}

void ConsoleOutputLogAppender::write(LogFormatter &format, LogEntry const &record) {
	std::cout << ConsoleTextColor::WHITE << "["
		<< std::setw(5)
		<< std::setfill(' ')
		<< record.threadId
		<< "] "
		<< record.timestamp
		<< std::setfill(' ')
		<< std::setw(5)
		<< LogLevelNameTable[record.level];

    switch (record.level) {
    case Fatal:
		std::cout << ConsoleTextColor::RED << " * "
			<< record.message;
        break;
    case Error:
        std::cout << ConsoleTextColor::DARKRED <<  " * "
			<< record.message;
        break;
    case Warn:
        std::cout << ConsoleTextColor::YELLOW <<  " * "
			<< record.message;
        break;
    case Info:
        std::cout << ConsoleTextColor::GREEN <<  " * "
			<< record.message;
        break;
    case Trace:
        std::cout << ConsoleTextColor::GRAY <<  " * "
			<< record.message;
        break;
    default:
        std::cout << ConsoleTextColor::WHITE <<  " * "
			<< record.message;
    }

    std::cout << ConsoleTextColor::WHITE << std::endl;
}

void ConsoleOutputLogAppender::setConsoleFont(std::wstring faceName, uint16_t fontSize) const {
    CONSOLE_FONT_INFOEX cfi = { 0 };
    cfi.cbSize = sizeof cfi;
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = fontSize;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    ::wcscpy_s(cfi.FaceName, faceName.c_str());
    ::SetCurrentConsoleFontEx(stdout_, FALSE, &cfi);
}

void ConsoleOutputLogAppender::setWindowSize(size_t width, size_t height) {
	CONSOLE_SCREEN_BUFFER_INFO oldConsoleScreenBufferInfo;
	if (!::GetConsoleScreenBufferInfo(stdout_, &oldConsoleScreenBufferInfo)) {
		throw Exception();
	}

	bool expand = false;
	if (oldConsoleScreenBufferInfo.dwSize.X < width) {
		expand = true;
		oldConsoleScreenBufferInfo.dwSize.X = width + 1;
	}

	if (oldConsoleScreenBufferInfo.dwSize.Y < height) {
		expand = true;
		oldConsoleScreenBufferInfo.dwSize.Y = height + 1;
	}

	if (expand) {
		if (!::SetConsoleScreenBufferSize(stdout_, oldConsoleScreenBufferInfo.dwSize)) {
			throw Exception();
		}
	}

	CONSOLE_SCREEN_BUFFER_INFO newConsoleScreenBufferInfo;
	if (!::GetConsoleScreenBufferInfo(stdout_, &newConsoleScreenBufferInfo)) {
		throw Exception();
	}

	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;

	if (width >= 80)
	{
		rect.Right = newConsoleScreenBufferInfo.dwMaximumWindowSize.X - 1;
	}
	else
	{
		rect.Right = (width << 1);
	}

	rect.Bottom = (height >> 1);

	if (!::SetConsoleWindowInfo(stdout_, TRUE, &rect)) {
		throw Exception();
	}
}

DebugOutputLogSink::~DebugOutputLogSink() {
}

void DebugOutputLogSink::write(LogFormatter &format, LogEntry const &record) {
	std::ostringstream ostr;
	format.format(ostr, record);
	::OutputDebugStringA(ostr.str().c_str());
}

FileLogAppender::FileLogAppender()
	: LogAppender()
	, limitLogSize_(LIMIT_FILE_LOG_SIZE)
	, logFileName_(L"server") {
	logStartDate_ = date::sys_days::clock::now();
}

void FileLogAppender::compressionDayBeforeLogFile() {
	namespace FS = std::tr2::sys;
	namespace ZIP = libzippp;

	date::year_month_day dayBefore(date::floor<date::days>(date::sys_days::clock::now() - date::days{ 1 }));

	std::wostringstream ostr;
	ostr << dayBefore;

	auto dayBeforeLogPath = ostr.str();

	std::tr2::sys::path dayBeforePath(logFilePath_ + dayBeforeLogPath);

	auto dayBeforeZipFullFilePath = dayBeforePath.wstring() + L".zip";

	if (FS::exists(dayBeforePath)) {
		return;
	}

	// 將前一天的資料夾壓縮
	ZIP::ZipArchive zipFile(utils::formUtf16(dayBeforeZipFullFilePath));

	zipFile.open(ZIP::ZipArchive::OpenMode::NEW);

	platform::MemoryMappedFile mappedFile(utils::formUtf16(dayBeforePath.wstring()
		+ L"\\" 
		+ logFileName_));
	
	// libzippp只支援'/'(Unix)資料路徑格式所以要轉換
	zipFile.addData(utils::formUtf16(dayBeforeLogPath)
		+ "/" + utils::formUtf16(logFileName_),
		mappedFile.data(),
		mappedFile.getFileSize());
}

void FileLogAppender::createLogFile() {
	namespace FS = std::tr2::sys;

	date::year_month_day now(date::floor<date::days>(date::sys_days::clock::now()));

	std::wostringstream ostr;
	ostr << now;
	auto nowDate = ostr.str();

	// Windows Vista之後支援'/'資料夾路徑, 但是std::tr2::sys都會轉為'\\'
	FS::path logFilePath(logFilePath_ + nowDate);

	if (!FS::exists(logFilePath)) {
		FS::create_directories(logFilePath);
	}

	logFilePath /= logFileName_;
	logging_.open(logFilePath.relative_path(), std::ios::binary | std::ios::app);
}

void FileLogAppender::setLimitLogSize(int size) {
	limitLogSize_ = size;
}

void FileLogAppender::truncate(size_t writtenSize) {
	date::year_month_day now(date::floor<date::days>(date::sys_days::clock::now()));
	date::year_month_day logStart(date::floor<date::days>(logStartDate_));

	if (logStart.day() != now.day()) {
		logging_.close();
		createLogFile();
		logStartDate_ = date::sys_days::clock::now();
	}

	if (static_cast<size_t>(logging_.tellp()) + writtenSize > limitLogSize_) {
		logging_.close();
		createLogFile();
	}
}

void FileLogAppender::setLogFileName(std::wstring const &logFileName) {
	logFileName_ = logFileName + L".log";
}

void FileLogAppender::setLogDirectory(std::wstring const &directory) {
	logFilePath_ = directory;
	createLogFile();
}

void FileLogAppender::write(LogFormatter &format, const LogEntry &entry) {
	std::ostringstream ostr;
	format.format(ostr, entry);
	ostr << "\r\n";
	auto str = ostr.str();
	truncate(str.length());
	logging_.write(str.c_str(), str.length());
	logging_.flush();
}

}

}
