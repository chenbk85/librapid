//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <map>

#include <windows.h>
#include <wincrypt.h>

#include "openssl.h"

std::map<std::string, APLNProtocols> const s_alpnProtocols{
	{ "http/1.1", ALPN_HTTP_V1_1 },
	{ "h2", ALPN_HTTP_V2 },
};

std::vector<uint8_t> getSHA1(std::string const &key) {
	std::vector<uint8_t> result(SHA_DIGEST_LENGTH);
	SHA1(reinterpret_cast<unsigned char const *>(key.c_str()), key.length(), result.data());
	return result;
}

std::string encodeBase64(std::string const& input) {
	auto pBase64Mem = ::BIO_new(::BIO_f_base64());
	auto pMem = ::BIO_new(::BIO_s_mem());

	pBase64Mem = ::BIO_push(pBase64Mem, pMem);

	::BIO_set_flags(pBase64Mem, BIO_FLAGS_BASE64_NO_NL); // 不要產生\n
	::BIO_write(pBase64Mem, input.c_str(), input.length());

	BIO_flush(pBase64Mem);
	BUF_MEM *pBufMem = nullptr;
	::BIO_get_mem_ptr(pBase64Mem, &pBufMem);
	std::vector<char> buffer(pBufMem->length + 1);
	memcpy(buffer.data(), pBufMem->data, pBufMem->length);
	::BIO_free_all(pBase64Mem);
	return buffer.data();
}

std::string decodeBase64(std::string const &input) {
	auto pBase64Mem = ::BIO_new(::BIO_f_base64());
	std::vector<char> buffer(input.length());
	auto pMemBuf = ::BIO_new_mem_buf((void*)input.c_str(), input.length());
	pMemBuf = ::BIO_push(pBase64Mem, pMemBuf);
	::BIO_read(pMemBuf, buffer.data(), input.length());
	::BIO_free_all(pMemBuf);
	return buffer.data();
}

void getSeed(char* seed, uint32_t length) {
	HCRYPTPROV hProvider = 0;
	::CryptAcquireContext(&hProvider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	::CryptGenRandom(hProvider, static_cast<DWORD>(length), reinterpret_cast<BYTE*>(seed));
	::CryptReleaseContext(hProvider, 0);
}

APLNProtocols fromString(std::string const &str) {
	auto itr = s_alpnProtocols.find(str);
	return (*itr).second;
}
