//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <rapid/utils/singleton.h>
#include "openssl.h"

class SSLContext;
	
class SSLManager : public rapid::utils::Singleton<SSLManager> {
public:
	SSLManager();

	SSLManager(SSLManager const &) = delete;
	SSLManager& operator=(SSLManager const &) = delete;

	void initializeServer(std::shared_ptr<SSLContext> pContext);

	std::shared_ptr<SSLContext> getDefaultSSLContext() const;

	void shutdown();
	
	void setALPNProtocol(APLNProtocols protocol);

	static int onAPLNSelect(SSL *pSSL,
		const unsigned char **out,
		unsigned char *outlen,
		const unsigned char *in,
		unsigned int inlen,
		void *arg);

private:
	static int onVerifyClient(int ok, X509_STORE_CTX* pStore);

	static int onPrivateKeyPassphrase(char* pBuf, int size, int flag, void* userData);

	std::function<bool()> verifyCallback_;
	std::shared_ptr<SSLContext> pDefaultServerContext_;
	std::string alpnProtocols_;
};
