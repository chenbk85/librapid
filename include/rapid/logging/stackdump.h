//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <string>
#include <cstdint>

#include <rapid/platform/platform.h>

namespace rapid {

namespace logging {

namespace details {

bool isKnownExceptionCode(uint32_t code);

std::string exceptionCodeToString(uint32_t code);

std::string dumpStackTrace();

std::string getStackTrace(EXCEPTION_POINTERS const *exceptionPtr);

std::string dumpStackTrace(CONTEXT const *context);

}

}

}


