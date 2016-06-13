//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <thread>
#include <string>

#include <rapid/utils/stopwatch.h>
#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

struct ThreadCpuUsage {
	ThreadCpuUsage() {
		usedSecond = 0;
		elapsedSecond = 0;
		usage = 0;
		kernelTimeStamp.dwHighDateTime = 0;
		kernelTimeStamp.dwLowDateTime = 0;
		userTimeStamp.dwHighDateTime = 0;
		userTimeStamp.dwLowDateTime = 0;
	}
	double usedSecond;
	double elapsedSecond;
	double usage;
	utils::SystemStopwatch stopwatch;
	FILETIME kernelTimeStamp;
	FILETIME userTimeStamp;
};

void setThreadName(std::thread* thread, std::string threadName);

void getThreadCpuUsage(uint32_t threadId, ThreadCpuUsage &usage);

uint32_t getThreadId(std::thread const * thread);

}

}
