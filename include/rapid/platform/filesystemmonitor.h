//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <queue>
#include <string>

#include <rapid/platform/platform.h>
#include <rapid/details/contracts.h>

namespace rapid {

namespace platform {

class FileSystemWatcher {
public:
	FileSystemWatcher(std::wstring const &path, bool recursive = true, int operationTowatch = -1);

	~FileSystemWatcher();

	FileSystemWatcher(FileSystemWatcher const &) = delete;
	FileSystemWatcher& operator=(FileSystemWatcher const &) = delete;

	std::wstring getChangedFile();
private:
	bool readChanged();
	void readChangedFile();

	// NOTICE: Must be 4 bytes aligned buffer
	static uint32_t constexpr BUFFER_SIZE = 16 * 1024;
	BOOL isRecursive_;
	DWORD operationTowatch_;
	HANDLE directory_;
	OVERLAPPED ov_;
	std::vector<BYTE> buffer_;
	std::queue<std::wstring> changedFiles_;
};

}

}
