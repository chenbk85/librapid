//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <concurrent_unordered_map.h>
#include <memory>
#include <vector>

#include <rapid/platform/slist.h>
#include <rapid/platform/spinlock.h>
#include <rapid/platform/memorymappedfile.h>
#include <rapid/iobuffer.h>

class HttpFileReader {
public:
	virtual ~HttpFileReader() = default;

	virtual bool isOpen() const = 0;

	virtual void close() = 0;

	virtual int64_t getFileSize() const = 0;

	virtual void seekTo(int64_t postion) = 0;

	virtual bool read(rapid::IoBuffer *pBuffer, uint32_t bufferLen) = 0;

	HttpFileReader(HttpFileReader const &) = delete;
	HttpFileReader& operator=(HttpFileReader const &) = delete;

protected:
	HttpFileReader() = default;
};

class HttpFileCacheReader final : public HttpFileReader {
public:
	HttpFileCacheReader();

	explicit HttpFileCacheReader(std::shared_ptr<std::vector<char>> const &fileCache);

	virtual ~HttpFileCacheReader() = default;

	HttpFileCacheReader(HttpFileCacheReader const &) = delete;
	HttpFileCacheReader& operator=(HttpFileCacheReader const &) = delete;

	void open(std::shared_ptr<std::vector<char>> const &fileCache);

	virtual bool isOpen() const override;

	virtual void close() override;

	virtual int64_t getFileSize() const override;

	virtual void seekTo(int64_t pos) override;

	virtual bool read(rapid::IoBuffer *pBuffer, uint32_t bufferLen) override;

private:
	std::shared_ptr<std::vector<char>> pFileCache_;
	int64_t position_;
};

class HttpMemoryFileReader final : public HttpFileReader {
public:
	HttpMemoryFileReader();

	explicit HttpMemoryFileReader(std::string const &filePath);

	void open(std::string const &filePath);

	virtual bool isOpen() const override;

	virtual void close() override;

	virtual int64_t getFileSize() const override;

	virtual void seekTo(int64_t postion) override;

	virtual bool read(rapid::IoBuffer *pBuffer, uint32_t bufferLen) override;

private:
	void remappedFile();

	rapid::platform::MemoryMappedFile mappedFile_;
	char const *data_;
	int64_t boundary_;
	int64_t fileSize_;
	int64_t position_;
};

class HttpSampleFileReader final : public HttpFileReader {
public:
	HttpSampleFileReader();

	explicit HttpSampleFileReader(std::string const &filePath);

	HttpSampleFileReader(HttpSampleFileReader const &) = delete;
	HttpSampleFileReader& operator=(HttpSampleFileReader const &) = delete;

	virtual ~HttpSampleFileReader();

	void open(std::string const &filePath);

	virtual bool isOpen() const override;

	virtual void close() override;

	virtual int64_t getFileSize() const override;

	virtual void seekTo(int64_t postion) override;

	virtual bool read(rapid::IoBuffer *pBuffer, uint32_t bufferLen) override;
private:
	HANDLE handle_;
};

class FileCacheManager {
public:
	struct HttpFileReaderCookie {
		HttpFileReaderCookie()
			: fileSize(0)
			, accessCount(0)
			, expiresTime(0) {
		}
		int64_t fileSize;
		int accessCount;
		time_t expiresTime;
		std::shared_ptr<rapid::platform::SList<HttpFileReader*>> pPool;
	};

	struct HttpFileReaderDeleter {
		inline void operator()(HttpFileReader* pHttpReader) const {
			auto pPool = pFileReaderPool.lock();
			if (!pPool) {
				pHttpReader->close();
			} else {
				pPool->enqueue(std::move(pHttpReader));
			}
		}
		std::weak_ptr<rapid::platform::SList<HttpFileReader*>> pFileReaderPool;
	};

	using HttpFileReaderPtr = std::unique_ptr<HttpFileReader, HttpFileReaderDeleter>;

	FileCacheManager();

	FileCacheManager(FileCacheManager const &) = delete;
	FileCacheManager& operator=(FileCacheManager const &) = delete;

	bool isExist(std::string const & filePath) const;

	std::shared_ptr<HttpFileReader> get(std::string const &filePath, bool compress = false);

private:
	static uint32_t constexpr CACHE_FILE_QUEUE_SIZE = 4096;
	static uint32_t constexpr CACHE_FILE_SIZE = 16 * 1024;
	static uint32_t constexpr NO_CACHE_SIZE = 10 * CACHE_FILE_SIZE;

	std::shared_ptr<std::vector<char>> getFromMemoryCache(std::string const &filePath, bool compress);

	static std::shared_ptr<HttpFileReader> getFileReaderFromPool(std::string const &filePath,
		HttpFileReaderCookie &cookie,
		bool compress);

	bool isExistCache(std::string const &filePath, HttpFileReaderCookie &fileReaderAccess);

	Concurrency::concurrent_unordered_map<std::string, std::shared_ptr<std::vector<char>>> memoryCacheMap_;
	Concurrency::concurrent_unordered_map<std::string, HttpFileReaderCookie> cookieMap_;
};
