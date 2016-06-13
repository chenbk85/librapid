//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

#include "openssl.h"
#include <rapid/connection.h>

class SSLException : public std::exception {
public:
	SSLException(SSL *ssl, int result);

	virtual ~SSLException();

	const char *what() const throw() override;
private:
	std::string message;
};

enum TLSResumptionState {
	TLS_RESUMPTION_STATE_REQUEST_SENT,
	TLS_RESUMPTION_STATE_RECORD,
	TLS_RESUMPTION_STATE_COMPLETE,
};

class SSLSessionCache;
	
class SSLContext {
public:
	SSLContext(std::string const &privateKeyFilePath, std::string const &certificateFilePath);

	SSLContext(SSLContext const &) = delete;
	SSLContext& operator=(SSLContext const &) = delete;

	~SSLContext();

	SSL* createSSL() const;

private:
	struct TLSTicket {
		uint8_t keyname[16];
		uint8_t aesKey[16];
		uint8_t hmacSecret[16];
	};

	void load(std::string const &privateKeyFilePath, std::string const &certificateFilePath);

	int onNewSessionCacheEntry(SSL *pSSL, SSL_SESSION *pSession);

	void onRemoveSessionCacheEntry(SSL_CTX *pCtx, SSL_SESSION *pSession);

	SSL_SESSION* onGetSessionCacheEntry(SSL *pSSL, unsigned char *data, int len, int *copy);

	int onTicketKey(SSL *pSSL,
		unsigned char *keyname,
		unsigned char *iv,
		EVP_CIPHER_CTX *ctx,
		HMAC_CTX *hctx,
		int enc);

	static SSL_SESSION * onGetSessionCacheEntryCallback(SSL *pSSL, unsigned char *data, int len, int *copy);

	static int onNewSessionCacheEntryCallback(SSL *pSSL, SSL_SESSION *pSession);

	static void onRemoveSessionCacheEntryCallback(SSL_CTX *pCtx, SSL_SESSION *pSession);

	static int onTicketKeyCallback(SSL *pSSL,
		unsigned char *keyname,
		unsigned char *iv,
		EVP_CIPHER_CTX *ctx, 
		HMAC_CTX *hctx,
		int enc);

	TLSTicket tlsTicket_;
	SSL_CTX *pSSLCtx_;
	std::unique_ptr<SSLSessionCache> pSessionCache_;
};

class OpenSslEngine {
public:
	explicit OpenSslEngine(SSLContext &context);

	OpenSslEngine(OpenSslEngine const &) = delete;
	OpenSslEngine& operator=(OpenSslEngine const &) = delete;

	~OpenSslEngine();

	void reset();

	APLNProtocols getALPN() const;

	bool handshake(rapid::IoBuffer *pSource, rapid::IoBuffer *pDest);

	bool decrypt(rapid::IoBuffer *pBuffer);

	void encrypt(rapid::IoBuffer *pBuffer);

private:
	static int onSelectSNIContextCallback(SSL* pSSL, int* ad, void* arg);

	int onSelectSNIContext(SSL* pSSL, int* ad, void* arg);

	bool writeFromBuffer(rapid::IoBuffer *pBuffer);

	void shutdown();

	bool isNegotiationFinished_ : 1;
	SSL *pSSL_;
	BIO* pInputBIO_;
	BIO* pOutputBIO_;
	TLSResumptionState resumptionState_;
};
