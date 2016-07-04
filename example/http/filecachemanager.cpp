//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <mutex>

#include <sys/stat.h>
#include <zlib.h>

#pragma comment(lib, "Shlwapi.lib")
#include <Shlwapi.h>

#include <rapid/logging/logging.h>
#include <rapid/platform/utils.h>

#include "httpserverconfigfacade.h"
#include "filecachemanager.h"

static std::string getTempFileName(std::string const &prefixString, bool unique) {
	char path[_MAX_PATH];
	path[0] = '\0';
	::GetTempPathA(MAX_PATH, path);
	::GetTempFileNameA(path, prefixString.c_str(), unique, path);
	return path;
}

static bool isFileExist(std::string const &filePath) {
#if	1
#if 0
	return ::PathFileExistsA(filePath.c_str()) != FALSE;
#else
	auto const fileAttr = ::GetFileAttributesA(filePath.c_str());
	if ((INVALID_FILE_ATTRIBUTES == fileAttr) || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
		return false;
	}
	return true;
#endif
#endif
	return true;
}

static void preReadFile(std::string const &filePath) {
	rapid::platform::MemoryMappedFile mappedFile(filePath);
	rapid::platform::prefetchVirtualMemory(mappedFile.data(), mappedFile.getFileSize());
}

static inline HANDLE getFileHandle(std::string const &filePath, bool readOnly = true) {
	DWORD flags = FILE_ATTRIBUTE_NORMAL;
	DWORD access = 0;
	DWORD creation = 0;
	DWORD shared = 0;

	if (readOnly) {
		access = GENERIC_READ;
		creation = OPEN_EXISTING;
		shared = FILE_SHARE_READ;
	} else {
		access = GENERIC_WRITE;
		creation = CREATE_ALWAYS;
		shared = 0;
	}

	auto fileHandle = ::CreateFileA(filePath.c_str(),
		access,
		shared,
		nullptr,
		creation,
		flags,
		nullptr);
	RAPID_ENSURE(fileHandle != INVALID_HANDLE_VALUE);
	return fileHandle;
}

static int64_t getFileSize(std::string const &filePath) {
	struct __stat64 fileStat;
	_stat64(filePath.c_str(), &fileStat);
	return fileStat.st_size;
}

static inline int64_t getFileSizeByHandle(HANDLE handle) {
	FILE_STANDARD_INFO fileInfo;
	if (!::GetFileInformationByHandleEx(handle, FileStandardInfo, &fileInfo, sizeof(fileInfo))) {
		throw rapid::Exception();
	}
	return fileInfo.EndOfFile.QuadPart;
}

static void gzipCompress(std::vector<char> const &input, std::vector<char> &ouput) {
	static auto constexpr GZIP_HEADER_SIZE = 16;
	static auto constexpr WINDOW_BIT = MAX_WBITS + 16;

	auto const inputSize = input.size();

	ouput.resize(GZIP_HEADER_SIZE + ::compressBound(inputSize));

	z_stream stream;
	memset(&stream, 0, sizeof(stream));

	auto retval = ::deflateInit2(&stream,
		Z_DEFAULT_COMPRESSION,
		Z_DEFLATED,
		WINDOW_BIT,
		Z_BEST_COMPRESSION,
		Z_DEFAULT_STRATEGY);
	RAPID_ENSURE(retval == Z_OK);

	std::unique_ptr<z_stream, decltype(&::deflateEnd)> pZStream(&stream, &::deflateEnd);

	stream.next_in = (Bytef*)input.data();
	stream.avail_in = input.size();
	stream.next_out = (Bytef*)ouput.data();
	stream.avail_out = static_cast<uInt>(ouput.size());

	gz_header gzip_header = { 0 };
	retval = ::deflateSetHeader(&stream, &gzip_header);
	RAPID_ENSURE(retval == Z_OK);

	retval = ::deflate(&stream, Z_FINISH);
	RAPID_ENSURE(retval == Z_STREAM_END);

	ouput.resize(stream.total_out);
}

static bool readFile(HANDLE fileHandle, std::shared_ptr<std::vector<char>> &pBuffer) {
	DWORD bytesRead = 0;

	auto retval = ::ReadFile(fileHandle, pBuffer->data(), pBuffer->size(), &bytesRead, nullptr);

	if (!retval) {
		throw rapid::Exception();
	}

	if (!bytesRead) {
		// 檔案讀取已達EOF.
		return true;
	}
	return false;
}

HttpFileCacheReader::HttpFileCacheReader()
	: position_(0) {
}

HttpFileCacheReader::HttpFileCacheReader(std::shared_ptr<std::vector<char>> const &fileCache)
	: pFileCache_(fileCache)
	, position_(0) {
}

void HttpFileCacheReader::open(std::shared_ptr<std::vector<char>> const& fileCache) {
	pFileCache_.reset();
	pFileCache_ = fileCache;
	position_ = 0;
}

bool HttpFileCacheReader::isOpen() const {
	return pFileCache_ != nullptr;
}

void HttpFileCacheReader::close() {
	pFileCache_.reset();
	position_ = 0;
}

int64_t HttpFileCacheReader::getFileSize() const {
	return pFileCache_->size();
}

void HttpFileCacheReader::seekTo(int64_t pos) {
	position_ = pos;
}

bool HttpFileCacheReader::read(rapid::IoBuffer *pBuffer, uint32_t bufferLen) {
	pBuffer->makeWriteableSpace(bufferLen);

	if (position_ == pFileCache_->size()) {
		return true;
	}

	uint32_t bytesRead = 0;
	if (position_ + bufferLen > pFileCache_->size()) {
		bytesRead = pFileCache_->size() - position_;
	}
	else {
		bytesRead = bufferLen;
	}

	memcpy(pBuffer->writeData(), pFileCache_->data() + position_, bytesRead);
	pBuffer->advanceWriteIndex(bytesRead);
	position_ += bytesRead;
	
	return false;
}

HttpSampleFileReader::HttpSampleFileReader()
	: handle_(INVALID_HANDLE_VALUE) {
}

HttpSampleFileReader::HttpSampleFileReader(std::string const& filePath) {
	open(filePath);
}

HttpSampleFileReader::~HttpSampleFileReader() {
	HttpSampleFileReader::close();
}

void HttpSampleFileReader::open(std::string const& filePath) {
	handle_ = ::CreateFileA(filePath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);
	RAPID_ENSURE(handle_ != INVALID_HANDLE_VALUE);
}

bool HttpSampleFileReader::isOpen() const {
	return handle_ != INVALID_HANDLE_VALUE;
}

void HttpSampleFileReader::close() {
	if (isOpen()) {
		::CloseHandle(handle_);
		handle_ = INVALID_HANDLE_VALUE;
	}
}

int64_t HttpSampleFileReader::getFileSize() const {
	return getFileSizeByHandle(handle_);
}

void HttpSampleFileReader::seekTo(int64_t postion) {
	LARGE_INTEGER largeInteger = { 0 };
	largeInteger.QuadPart = postion;
	if (!::SetFilePointerEx(handle_, largeInteger, nullptr, FILE_BEGIN)) {
		throw rapid::Exception();
	}
}

bool HttpSampleFileReader::read(rapid::IoBuffer* pBuffer, uint32_t bufferLen) {
	DWORD bytesRead = 0;

	pBuffer->makeWriteableSpace(bufferLen);

	auto retval = ::ReadFile(handle_, pBuffer->writeData(), bufferLen, &bytesRead, nullptr);
	if (!retval) {
		throw rapid::Exception();
	}

	if (!bytesRead) {
		return true;
	}
	else {
		pBuffer->advanceWriteIndex(bytesRead);
	}
	return false;
}

// Sample Win32 File API Wrapper
class File {
public:
	File(std::string const &filePath, bool readOnly) {
		fileHandle_ = getFileHandle(filePath, readOnly);
		fileSize_ = getFileSizeByHandle(fileHandle_);
	}

	File(File const &) = delete;
	File& operator=(File const &) = delete;

	~File() noexcept {
		if (fileHandle_ != INVALID_HANDLE_VALUE) {
			::CloseHandle(fileHandle_);
		}
	}

	size_t fileSize() const noexcept {
		return fileSize_;
	}

	std::shared_ptr<std::vector<char>> read() {
		auto pFileCache = std::make_shared<std::vector<char>>(fileSize_);
		readFile(fileHandle_, pFileCache);
		return pFileCache;
	}

	void write(std::shared_ptr<std::vector<char>> const &buffer) {
		DWORD written = 0;
		if (!::WriteFile(fileHandle_, buffer->data(), buffer->size(), &written, nullptr)) {
			throw rapid::Exception();
		}
	}

	static void compressFile(std::string const &compressFilePath, std::string const &destFilePath) {
		File reader(compressFilePath, true);
		File writter(destFilePath, false);
		auto pCompressFileCache = std::make_shared<std::vector<char>>();
		gzipCompress(*reader.read(), *pCompressFileCache);
		writter.write(pCompressFileCache);
	}
private:
	HANDLE fileHandle_;
	size_t fileSize_;
};

FileCacheManager::FileCacheManager() {

}

std::shared_ptr<std::vector<char>> FileCacheManager::getFromMemoryCache(std::string const& filePath, bool compress) {
	auto cache = memoryCacheMap_.find(filePath);
	if (cache != memoryCacheMap_.end()) {
		return (*cache).second;
	}

	File reader(filePath, true);

	auto pFileCache = reader.read();

	if (compress) {
		auto pCompressFileCache = std::make_shared<std::vector<char>>();
		gzipCompress(*pFileCache, *pCompressFileCache);

		memoryCacheMap_[filePath] = pCompressFileCache;
		return pCompressFileCache;
	} else {
		memoryCacheMap_[filePath] = pFileCache;
	}
	return pFileCache;
}

bool FileCacheManager::isExist(std::string const & filePath) const {
	if (cookieMap_.find(filePath) != cookieMap_.end()) {
		return true;
	}
	return isFileExist(filePath);
}

bool FileCacheManager::isExistCache(std::string const & filePath, HttpFileReaderCookie &fileReaderAccess) {
	auto itr = cookieMap_.find(filePath);
	if (itr != cookieMap_.end()) {
		fileReaderAccess = (*itr).second;
		return true;
	}
	return false;
}

std::shared_ptr<HttpFileReader> FileCacheManager::get(std::string const &filePath, bool compress) {
	HttpFileReaderCookie readerCookie;

	if (!isExistCache(filePath, readerCookie)) {
		readerCookie.pPool = std::make_shared<rapid::platform::SList<HttpFileReader*>>();
		readerCookie.fileSize = getFileSize(filePath);
		cookieMap_[filePath] = readerCookie;
	}

	if (readerCookie.fileSize <= CACHE_FILE_SIZE) {
		return std::make_unique<HttpFileCacheReader>(getFromMemoryCache(filePath, compress));
	}
	else {
		return getFileReaderFromPool(filePath, readerCookie, compress);
	}
}

std::shared_ptr<HttpFileReader> FileCacheManager::getFileReaderFromPool(std::string const &filePath,
	HttpFileReaderCookie &cookie,
	bool compress) {

	HttpFileReaderDeleter deleter;

	deleter.pFileReaderPool = cookie.pPool;

	HttpFileReader *fileReader = nullptr;

	if (!cookie.pPool->tryDequeue(fileReader)) {
		if (compress) {
			auto tempFileName = getTempFileName("compress", false);
			File::compressFile(filePath, tempFileName);
			// 將檔案預讀到記憶體上
			preReadFile(tempFileName);
			return HttpFileReaderPtr(new HttpSampleFileReader(tempFileName), deleter);
		} else {
			if (cookie.fileSize <= NO_CACHE_SIZE) {
				preReadFile(filePath);
			} else if (HttpServerConfigFacade::getInstance().getBufferSize() > SIZE_64KB) {
				// 實驗性值:
				// 使用Memory mapped file來讀取檔案, 但是需要緩衝區大於64KB以上
				return HttpFileReaderPtr(new HttpMemoryFileReader(filePath), deleter);
			}
			return HttpFileReaderPtr(new HttpSampleFileReader(filePath), deleter);
		}
	}

	fileReader->seekTo(0);
	return HttpFileReaderPtr(fileReader, deleter);
}

HttpMemoryFileReader::HttpMemoryFileReader()
	: position_(0)
	, fileSize_(0)
	, boundary_(0)
	, data_(nullptr) {
}

HttpMemoryFileReader::HttpMemoryFileReader(std::string const & filePath)
	: HttpMemoryFileReader() {
	open(filePath);
}

void HttpMemoryFileReader::open(std::string const &filePath) {
	fileSize_ = ::getFileSize(filePath);
	mappedFile_.open(filePath, false);
	remappedFile();
}

void HttpMemoryFileReader::remappedFile() {
	auto remain = (std::min)(SIZE_10MB, static_cast<uint32_t>(fileSize_ - position_));
	boundary_ += remain;
	mappedFile_.map(position_, remain);
	data_ = mappedFile_.data();
}

bool HttpMemoryFileReader::isOpen() const {
	return true;
}

void HttpMemoryFileReader::close() {
	mappedFile_.close();
	fileSize_ = 0;
	boundary_ = 0;
	data_ = nullptr;
}

int64_t HttpMemoryFileReader::getFileSize() const {
	return fileSize_;
}

void HttpMemoryFileReader::seekTo(int64_t postion) {
	boundary_ = 0;
	position_ = postion;
	remappedFile();
}

bool HttpMemoryFileReader::read(rapid::IoBuffer * pBuffer, uint32_t bufferLen) {
	if (bufferLen < SIZE_4KB) {
		return false;
	}

	pBuffer->makeWriteableSpace(SIZE_64KB);
	bufferLen = SIZE_64KB;

	if (position_ == fileSize_) {
		return true;
	}

	uint32_t bytesRead = 0;
	if (position_ + bufferLen > fileSize_) {
		bytesRead = fileSize_ - position_;
	}
	else {
		bytesRead = bufferLen;
	}

	if (position_ + bytesRead > boundary_) {
		remappedFile();
	}

	memcpy(pBuffer->writeData(), data_, bytesRead);
	pBuffer->advanceWriteIndex(bytesRead);
	position_ += bytesRead;
	data_ += bytesRead;

	return false;
}
