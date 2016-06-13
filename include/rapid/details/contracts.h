//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <intrin.h>

#include <rapid/exception.h>

#define RAPID_ENSURE(expr)  \
	if (!(expr)) {\
		__debugbreak(); \
		throw rapid::ExceptionWithMinidump("*** Contract violation: "#expr" is false", __FILE__, __LINE__);\
	}
