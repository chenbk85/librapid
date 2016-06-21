//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <string>
#include <map>

#include <rapid/logging/logging.h>
#include "sslcontext.h"
#include "sslmanager.h"

static std::map<APLNProtocols, std::string> const s_alpnProtocols {
	{ ALPN_HTTP_V1_1, "http/1.1" },
	{ ALPN_HTTP_V2, "h2" },
};

SSLManager::SSLManager() {
}

void SSLManager::initializeServer(std::shared_ptr<SSLContext> pContext) {
	pDefaultServerContext_ = pContext;
}

void SSLManager::shutdown() {
}

std::shared_ptr<SSLContext> SSLManager::getDefaultSSLContext() const {
	return pDefaultServerContext_;
}

int SSLManager::onVerifyClient(int ok, X509_STORE_CTX* pStore) {
	return -1;
}

int SSLManager::onPrivateKeyPassphrase(char* pBuf, int size, int flag, void* userData) {
	return -1;
}

void SSLManager::setALPNProtocol(APLNProtocols protocol) {
	auto itr = s_alpnProtocols.find(protocol);
	alpnProtocols_.append((*itr).second);
}

int SSLManager::onAPLNSelect(SSL* pSSL,
		const unsigned char** out,
		unsigned char* outlen,
		const unsigned char* in, 
		unsigned inlen, 
		void* arg) {
	auto *pSSLManager = reinterpret_cast<SSLManager*>(arg);
	auto end = in + inlen;
	for (; end > in;) {
		int protoLen = *in;
		std::string const protocol(reinterpret_cast<char const*>(in + 1), protoLen);
		RAPID_LOG_TRACE_STACK_TRACE() << "APLN: Client support protocol= " << protocol;
		if (protocol == pSSLManager->alpnProtocols_) {
			*out = reinterpret_cast<const unsigned char*>(pSSLManager->alpnProtocols_.data());
			*outlen = pSSLManager->alpnProtocols_.length();
			RAPID_LOG_TRACE_STACK_TRACE() << "APLN: Sever select protocol= " << protocol;
			return SSL_TLSEXT_ERR_OK;
		}
		in += protoLen + 1;
	}
	return SSL_TLSEXT_ERR_NOACK;
}

