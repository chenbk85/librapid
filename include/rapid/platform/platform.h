//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <VersionHelpers.h>

typedef int socklen_t;
typedef char raw_type;

#include <Winsock2.h>

auto constexpr CACHE_LINE_PAD_SIZE = 128;
auto constexpr MAX_CACHE_LINE_PAD_SIZE = 4096;
