//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

class MemoryMappedFile {
public:
	MemoryMappedFile();

	MemoryMappedFile(std::string const &filePath, bool mapAllFileSize = true);

	MemoryMappedFile(MemoryMappedFile const &) = delete;
	MemoryMappedFile& operator=(MemoryMappedFile const &) = delete;

	~MemoryMappedFile() noexcept;

	void open(std::string const &filePath, bool mapAllFileSize);

	void map(int64_t position, uint32_t length);

	char const * data() const noexcept;

	int64_t getMappedSize() const;

	int64_t getFileSize() const;

	void close() noexcept;
private:
	HANDLE mapping_;
	char *data_;
	int64_t fileSize_;
};

}

}
