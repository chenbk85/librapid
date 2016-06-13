//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <memory>

#include <rapid/details/contracts.h>
#include <rapid/platform/threadutils.h>

namespace rapid {

namespace platform {

static inline void setThreadName(uint32_t dwThreadID, char const  *threadName) {
	static uint32_t const MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
	typedef struct tagTHREADNAME_INFO {
		uint32_t dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		uint32_t dwThreadID; // Thread ID (-1=caller thread).
		uint32_t dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try {
		::RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

uint32_t getThreadId(std::thread const * thread) {
	return ::GetThreadId(static_cast<HANDLE>(const_cast<std::thread*>(thread)->native_handle()));
}

void setThreadName(std::thread* thread, std::string threadName) {
	RAPID_ENSURE(thread != nullptr);
	auto threadId = getThreadId(thread);
	setThreadName(threadId, threadName.c_str());
}

static int64_t fileTimeTo100nsec(FILETIME const &fileTime) {
	auto n = fileTime.dwHighDateTime;
	n <<= 32;
	n += fileTime.dwLowDateTime;
	return n;
}

static double diffFileTime(FILETIME const &startFiletime, FILETIME const &endFiletime) {
	auto start = fileTimeTo100nsec(startFiletime);
	auto end = fileTimeTo100nsec(endFiletime);
	return static_cast<double>(end - start) / 1.0e7;
}

static void getThreadCpuUsage(HANDLE thread, ThreadCpuUsage &usage) {
	FILETIME createdTimeStamp = { 0 };
	FILETIME exitedTimeStamp = { 0 };
	FILETIME kernelTimeStamp = { 0 };
	FILETIME userTimeStamp = { 0 };

	if (!::GetThreadTimes(thread, &createdTimeStamp, &exitedTimeStamp, &kernelTimeStamp, &userTimeStamp)) {
		throw Exception();
	}

	auto usertimeSecond = diffFileTime(usage.userTimeStamp, userTimeStamp);
	usage.userTimeStamp = userTimeStamp;
	auto kerneltimeSecond = diffFileTime(usage.kernelTimeStamp, kernelTimeStamp);
	usage.kernelTimeStamp = kernelTimeStamp;
	auto usedSecond = static_cast<double>(kerneltimeSecond + usertimeSecond);
	auto passedSecond = 
		static_cast<double>(usage.stopwatch.elapsedCount<std::chrono::milliseconds>()) / 1000;
	usage.stopwatch.reset();
	usage.elapsedSecond = passedSecond;
	usage.usedSecond = usedSecond;
	usage.usage = usedSecond / passedSecond;
}

void getThreadCpuUsage(uint32_t threadId, ThreadCpuUsage &usage) {
	std::unique_ptr<void, decltype(&::CloseHandle)>
		handle(::OpenThread(THREAD_QUERY_INFORMATION, false, threadId), &::CloseHandle);
	return getThreadCpuUsage(handle.get(), usage);
}

}

}
