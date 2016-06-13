//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <rapid/platform/platform.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

enum APLNProtocols : uint8_t {
	ALPN_HTTP_V1_1 = 0,
	ALPN_HTTP_V2,
};

std::string encodeBase64(std::string const &input);

std::string decodeBase64(std::string const &input);

void getSeed(char* seed, uint32_t length);

APLNProtocols fromString(std::string const &str);

std::vector<uint8_t> getSHA1(std::string const &key);
