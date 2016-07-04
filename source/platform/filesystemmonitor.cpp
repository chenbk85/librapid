//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>
#include <rapid/platform/filesystemmonitor.h>

namespace rapid {
namespace platform {

FileSystemWatcher::FileSystemWatcher(std::wstring const &path, bool recursive, int operationTowatch)
	: buffer_(BUFFER_SIZE) {
	if (operationTowatch == -1) {
		operationTowatch_ = FILE_NOTIFY_CHANGE_LAST_WRITE;
	}

	directory_ = ::CreateFileW(path.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		// ReadDirectoryChangesW() needs FILE_FLAG_BACKUP_SEMANTICS
		FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS,
		nullptr);

	RAPID_ENSURE(directory_ != INVALID_HANDLE_VALUE);
	memset(&ov_, 0, sizeof(ov_));
	readChanged();
}

FileSystemWatcher::~FileSystemWatcher() {
	if (directory_ != INVALID_HANDLE_VALUE) {
		::CancelIo(directory_);
		::CloseHandle(directory_);
	}
}

bool FileSystemWatcher::readChanged() {
	auto ret = ::ReadDirectoryChangesW(directory_,
		buffer_.data(),
		buffer_.size(),
		isRecursive_,
		operationTowatch_,
		nullptr,
		&ov_,
		nullptr);

	if (!ret) {
		auto lastError = ::GetLastError();
		if (lastError == ERROR_IO_PENDING) {
			return false;
		} else {
			throw Exception(lastError);
		}
	}
	return true;
}

void FileSystemWatcher::readChangedFile() {
	auto p = reinterpret_cast<FILE_NOTIFY_INFORMATION const *>(buffer_.data());

	for (;; p = reinterpret_cast<FILE_NOTIFY_INFORMATION const *>((char*)p + p->NextEntryOffset)) {
		std::wstring fileName(p->FileName, p->FileNameLength / sizeof(wchar_t));

		if (changedFiles_.empty() || fileName != changedFiles_.back())
			changedFiles_.push(std::move(fileName));

		if (!p->NextEntryOffset)
			break;
	}
}

std::wstring FileSystemWatcher::getChangedFile() {
	while (true) {
		DWORD bytesRetured = 0;

		if (!::GetOverlappedResult(directory_, &ov_, &bytesRetured, FALSE))
			break;

		readChangedFile();

		memset(&ov_, 0, sizeof(ov_));
		readChanged();
	}

	if (!changedFiles_.empty()) {
		auto changedFile = changedFiles_.front();
		changedFiles_.pop();
		return changedFile;
	}
	return L"";
}


}
}
