//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <memory>
#include <mutex>

#include <rapid/details/socketaddress.h>
#include <rapid/iobuffer.h>

#include <rapid/utils/singleton.h>
#include <rapid/utils/stringutilis.h>

#include <rapid/details/contracts.h>

#include <rapid/logging/logging.h>
#include <rapid/platform/spinlock.h>

#include "sslmanager.h"
#include "sslcontext.h"

static char const * getDefaultCipherSuites() {
	static char const s_defaultCipherSuites[] = {
		"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-"
		"AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:"
		"DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-"
		"AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-"
		"AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-"
		"AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:"
		"DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:"
		"!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK"
	};
	return s_defaultCipherSuites;
}

struct DH2048 {
	static uint8_t const P[];
	static uint8_t const G[];
};

uint8_t const DH2048::P[] = {
	0xF6, 0x42, 0x57, 0xB7, 0x08, 0x7F, 0x08, 0x17, 0x72, 0xA2, 0xBA, 0xD6,
	0xA9, 0x42, 0xF3, 0x05, 0xE8, 0xF9, 0x53, 0x11, 0x39, 0x4F, 0xB6, 0xF1,
	0x6E, 0xB9, 0x4B, 0x38, 0x20, 0xDA, 0x01, 0xA7, 0x56, 0xA3, 0x14, 0xE9,
	0x8F, 0x40, 0x55, 0xF3, 0xD0, 0x07, 0xC6, 0xCB, 0x43, 0xA9, 0x94, 0xAD,
	0xF7, 0x4C, 0x64, 0x86, 0x49, 0xF8, 0x0C, 0x83, 0xBD, 0x65, 0xE9, 0x17,
	0xD4, 0xA1, 0xD3, 0x50, 0xF8, 0xF5, 0x59, 0x5F, 0xDC, 0x76, 0x52, 0x4F,
	0x3D, 0x3D, 0x8D, 0xDB, 0xCE, 0x99, 0xE1, 0x57, 0x92, 0x59, 0xCD, 0xFD,
	0xB8, 0xAE, 0x74, 0x4F, 0xC5, 0xFC, 0x76, 0xBC, 0x83, 0xC5, 0x47, 0x30,
	0x61, 0xCE, 0x7C, 0xC9, 0x66, 0xFF, 0x15, 0xF9, 0xBB, 0xFD, 0x91, 0x5E,
	0xC7, 0x01, 0xAA, 0xD3, 0x5B, 0x9E, 0x8D, 0xA0, 0xA5, 0x72, 0x3A, 0xD4,
	0x1A, 0xF0, 0xBF, 0x46, 0x00, 0x58, 0x2B, 0xE5, 0xF4, 0x88, 0xFD, 0x58,
	0x4E, 0x49, 0xDB, 0xCD, 0x20, 0xB4, 0x9D, 0xE4, 0x91, 0x07, 0x36, 0x6B,
	0x33, 0x6C, 0x38, 0x0D, 0x45, 0x1D, 0x0F, 0x7C, 0x88, 0xB3, 0x1C, 0x7C,
	0x5B, 0x2D, 0x8E, 0xF6, 0xF3, 0xC9, 0x23, 0xC0, 0x43, 0xF0, 0xA5, 0x5B,
	0x18, 0x8D, 0x8E, 0xBB, 0x55, 0x8C, 0xB8, 0x5D, 0x38, 0xD3, 0x34, 0xFD,
	0x7C, 0x17, 0x57, 0x43, 0xA3, 0x1D, 0x18, 0x6C, 0xDE, 0x33, 0x21, 0x2C,
	0xB5, 0x2A, 0xFF, 0x3C, 0xE1, 0xB1, 0x29, 0x40, 0x18, 0x11, 0x8D, 0x7C,
	0x84, 0xA7, 0x0A, 0x72, 0xD6, 0x86, 0xC4, 0x03, 0x19, 0xC8, 0x07, 0x29,
	0x7A, 0xCA, 0x95, 0x0C, 0xD9, 0x96, 0x9F, 0xAB, 0xD0, 0x0A, 0x50, 0x9B,
	0x02, 0x46, 0xD3, 0x08, 0x3D, 0x66, 0xA4, 0x5D, 0x41, 0x9F, 0x9C, 0x7C,
	0xBD, 0x89, 0x4B, 0x22, 0x19, 0x26, 0xBA, 0xAB, 0xA2, 0x5E, 0xC3, 0x55,
	0xE9, 0x32, 0x0B, 0x3B,
};

uint8_t const DH2048::G[] = {
	0x02
};

SSLException::SSLException(SSL *ssl, int result) {
	auto error = ::SSL_get_error(ssl, result);
	if (error == SSL_ERROR_NONE) {
		message.assign("Unknown SSL error");
		return;
	}

	auto errorlog = error;
	char msg[1024] = { 0 };
	while (SSL_ERROR_NONE != errorlog) {
		::ERR_error_string_n(errorlog, msg, _countof(msg));
		errorlog = ::ERR_get_error();
		message.append(msg);
	}
}

SSLException::~SSLException() {
}

const char *SSLException::what() const throw() {
	return message.c_str();
}

static std::vector<uint8_t> getASN1(SSL_SESSION *pSession) {
	std::vector<uint8_t> buffer(::i2d_SSL_SESSION(pSession, nullptr));
	auto data = buffer.data();
	::i2d_SSL_SESSION(pSession, &data);
	return buffer;
}

static std::string getSessionId(SSL_SESSION const *pSession) {
	std::vector<uint8_t> buffer(pSession->session_id_length);
	memcpy(buffer.data(), pSession->session_id, buffer.size());
	return rapid::utils::bin2hex(buffer);
}

class SSLSessionCache {
public:
	SSLSessionCache() {
	}

	SSLSessionCache(SSLSessionCache const &) = delete;
	SSLSessionCache& operator=(SSLSessionCache const &) = delete;

	SSL_SESSION* getSession(std::string const &sessionId) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto itr = sessionMap_.find(sessionId);
		if (itr != sessionMap_.end()) {
			uint8_t const * data = (*itr).second.data();
			return ::d2i_SSL_SESSION(nullptr, &data, (*itr).second.size());
		}
		return nullptr;
	}

	void addSession(SSL_SESSION *pSession) {
		auto sessionId = getSessionId(pSession);
		RAPID_LOG_INFO() << "[SSL] Add session id: " << sessionId;

		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto itr = sessionMap_.insert(std::make_pair(sessionId, getASN1(pSession)));
		if (itr.second) {
			itr.first->second = getASN1(pSession);
		}
	}

	void removeSession(SSL_SESSION *pSession) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto sessionId = getSessionId(pSession);
		RAPID_LOG_INFO() << "[SSL] Remove session id: " << sessionId;
		sessionMap_.erase(sessionId);
	}

	void clear() {
		sessionMap_.clear();
	}

private:
	using SessionMap = std::unordered_map<std::string, std::vector<uint8_t>>;
	SessionMap sessionMap_;
	mutable rapid::platform::Spinlock lock_;
};

template <typename DHCode>
static inline std::unique_ptr<DH, decltype(&::DH_free)> getDh() {
	std::unique_ptr<DH, decltype(&::DH_free)> dh(::DH_new(), &::DH_free);
	if (!dh) {
		throw rapid::Exception(::GetLastError());
	}
	dh->p = ::BN_bin2bn(DHCode::P, sizeof(DHCode::P), nullptr);
	dh->g = ::BN_bin2bn(DHCode::G, sizeof(DHCode::G), nullptr);
	if (!dh->p || !dh->g) {
		throw rapid::Exception();
	}
	dh->length = 160;
	return dh;
}

static std::unique_ptr<EC_KEY, decltype(&::EC_KEY_free)> makeEcKey() {
	return std::unique_ptr<EC_KEY, decltype(&::EC_KEY_free)>(
		::EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), &::EC_KEY_free);
}

SSLContext::SSLContext(std::string const &privateKeyFilePath, std::string const &certificateFilePath)
	: pSessionCache_(new SSLSessionCache())
	, pSSLCtx_(nullptr) {
	load(privateKeyFilePath, certificateFilePath);
}

SSL* SSLContext::createSSL() const {
	return ::SSL_new(pSSLCtx_);
}

void SSLContext::load(std::string const &privateKeyFilePath, std::string const &certificateFilePath) {
	pSSLCtx_ = ::SSL_CTX_new(::TLSv1_2_server_method());

	// Read private key file and certificate file
	auto retval = 0;
	if (!privateKeyFilePath.empty()) {
		retval = ::SSL_CTX_use_PrivateKey_file(pSSLCtx_, privateKeyFilePath.c_str(), SSL_FILETYPE_PEM);
		RAPID_ENSURE(retval == 1);
	}
	
	if (!certificateFilePath.empty()) {
		retval = ::SSL_CTX_use_certificate_file(pSSLCtx_, certificateFilePath.c_str(), SSL_FILETYPE_PEM);
		RAPID_ENSURE(retval == 1);
	}

	retval = ::SSL_CTX_check_private_key(pSSLCtx_);
	RAPID_ENSURE(retval == 1);

	retval = ::SSL_CTX_set_cipher_list(pSSLCtx_, getDefaultCipherSuites());
	RAPID_ENSURE(retval == 1);
	
	auto dh = getDh<DH2048>();
	retval = ::SSL_CTX_set_tmp_dh(pSSLCtx_, dh.get());
	RAPID_ENSURE(retval == 1);
	
	auto ecdh = makeEcKey();
	retval = ::SSL_CTX_set_tmp_ecdh(pSSLCtx_, ecdh.get());
	RAPID_ENSURE(retval == 1);
	
	::SSL_CTX_set_verify_depth(pSSLCtx_, 10);
	
	// Server side options
	::SSL_CTX_set_options(pSSLCtx_, (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS));
	::SSL_CTX_set_options(pSSLCtx_, SSL_OP_NO_COMPRESSION);
	::SSL_CTX_set_options(pSSLCtx_, SSL_OP_NO_SSLv2);
	::SSL_CTX_set_options(pSSLCtx_, SSL_OP_NO_SSLv3);
	::SSL_CTX_set_options(pSSLCtx_, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
	::SSL_CTX_set_mode(pSSLCtx_, SSL_MODE_AUTO_RETRY);
	::SSL_CTX_set_mode(pSSLCtx_, SSL_MODE_RELEASE_BUFFERS);
	::SSL_CTX_set_verify(pSSLCtx_, SSL_VERIFY_NONE, nullptr);
	::SSL_CTX_set_options(pSSLCtx_, SSL_OP_CIPHER_SERVER_PREFERENCE);

	// Session cache setting
	::SSL_CTX_set_session_cache_mode(pSSLCtx_, SSL_SESS_CACHE_NO_AUTO_CLEAR);
	::SSL_CTX_set_session_cache_mode(pSSLCtx_, SSL_SESS_CACHE_NO_INTERNAL_LOOKUP);
	::SSL_CTX_set_session_cache_mode(pSSLCtx_, SSL_SESS_CACHE_SERVER);
	::SSL_CTX_sess_set_new_cb(pSSLCtx_, onNewSessionCacheEntryCallback);
	::SSL_CTX_sess_set_get_cb(pSSLCtx_, onGetSessionCacheEntryCallback);
	::SSL_CTX_sess_set_remove_cb(pSSLCtx_, onRemoveSessionCacheEntryCallback);

	// Setup session ticket
	getSeed(reinterpret_cast<char*>(&tlsTicket_), sizeof(tlsTicket_));
	::SSL_CTX_set_tlsext_ticket_key_cb(pSSLCtx_, onTicketKeyCallback);

	// Http2 future
	::SSL_CTX_set_alpn_select_cb(pSSLCtx_, SSLManager::onAPLNSelect, &SSLManager::getInstance());
}

SSL_SESSION * SSLContext::onGetSessionCacheEntryCallback(SSL *pSSL, unsigned char *data, int len, int *copy) {
	return SSLManager::getInstance().getDefaultSSLContext()->onGetSessionCacheEntry(pSSL, data, len, copy);
}

int SSLContext::onNewSessionCacheEntryCallback(SSL *pSSL, SSL_SESSION *pSession) {
	return SSLManager::getInstance().getDefaultSSLContext()->onNewSessionCacheEntry(pSSL, pSession);
}

int SSLContext::onTicketKeyCallback(SSL *pSSL, unsigned char *key_name, unsigned char *iv, EVP_CIPHER_CTX *ctx, HMAC_CTX *hctx, int enc) {
	return SSLManager::getInstance().getDefaultSSLContext()->onTicketKey(pSSL, key_name, iv, ctx, hctx, enc);
}

void SSLContext::onRemoveSessionCacheEntryCallback(SSL_CTX *pCtx, SSL_SESSION *pSession) {
	return SSLManager::getInstance().getDefaultSSLContext()->onRemoveSessionCacheEntry(pCtx, pSession);
}

int SSLContext::onNewSessionCacheEntry(SSL *pSSL, SSL_SESSION *pSession) {
	pSessionCache_->addSession(pSession);
	return 1;
}

void SSLContext::onRemoveSessionCacheEntry(SSL_CTX *pCtx, SSL_SESSION *pSession) {
	pSessionCache_->removeSession(pSession);
}

int SSLContext::onTicketKey(SSL *pSSL, unsigned char *keyname, unsigned char *iv, EVP_CIPHER_CTX *ctx, HMAC_CTX *hctx, int enc) {
	// Create new session
	if (enc == 1) {
		RAPID_LOG_TRACE() << "[SSL] Create new TLS session ticket";
		memcpy(keyname, tlsTicket_.keyname, sizeof(tlsTicket_.keyname));
		::RAND_pseudo_bytes(iv, EVP_MAX_IV_LENGTH);
		::EVP_EncryptInit_ex(ctx, ::EVP_aes_128_cbc(), nullptr, tlsTicket_.aesKey, iv);
		::HMAC_Init_ex(hctx, tlsTicket_.hmacSecret, 16, ::EVP_sha256(), nullptr);
		return 0;
	} else {
	//  Retrieve session
		if (memcmp(keyname, tlsTicket_.keyname, sizeof(tlsTicket_.keyname)) != 0) {
			RAPID_LOG_TRACE() << "[SSL] Incorrect TLS session ticket";
			return 0;
		}
		RAPID_LOG_TRACE() << "[SSL] Correct TLS session ticket";
		::EVP_DecryptInit_ex(ctx, ::EVP_aes_128_cbc(), nullptr, tlsTicket_.aesKey, iv);
		::HMAC_Init_ex(hctx, tlsTicket_.hmacSecret, sizeof(tlsTicket_.hmacSecret), ::EVP_sha256(), nullptr);
		return 1;
	}
	return -1;
}

SSL_SESSION* SSLContext::onGetSessionCacheEntry(SSL *pSSL, unsigned char *key, int len, int *copy) {
	std::vector<uint8_t> buffer(len);
	memcpy(buffer.data(), key, len);
	auto pSession = pSessionCache_->getSession(rapid::utils::bin2hex(buffer));
	if (pSession != nullptr) {
		RAPID_LOG_TRACE() << "[SSL] Get session id: " << rapid::utils::bin2hex(buffer);
	}
	*copy = 0;
	return pSession;
}

SSLContext::~SSLContext() {
	if (!pSSLCtx_) {
		return;
	}
	::SSL_CTX_free(pSSLCtx_);
}

OpenSslEngine::OpenSslEngine(SSLContext &context)
	: isNegotiationFinished_(false)
	, pSSL_(context.createSSL())
	, pInputBIO_(::BIO_new(::BIO_s_mem()))
	, pOutputBIO_(::BIO_new(::BIO_s_mem())) {

	::SSL_set_bio(pSSL_, pInputBIO_, pOutputBIO_);

	auto mode = ::SSL_get_mode(pSSL_);
	::SSL_set_mode(pSSL_, mode | SSL_MODE_RELEASE_BUFFERS);
}

int OpenSslEngine::onSelectSNIContext(SSL* pSSL, int* ad, void* arg) {
	auto servername = ::SSL_get_servername(pSSL, TLSEXT_NAMETYPE_host_name);
	if (servername == nullptr)
		return SSL_TLSEXT_ERR_OK;
	return SSL_TLSEXT_ERR_OK;
}

int OpenSslEngine::onSelectSNIContextCallback(SSL* pSSL, int* ad, void* arg) {
	auto pEngine = static_cast<OpenSslEngine*>(SSL_get_app_data(pSSL));
	return pEngine->onSelectSNIContext(pSSL, ad, arg);
}

APLNProtocols OpenSslEngine::getALPN() const {
	unsigned char const *data = nullptr;
	uint32_t len = 0;
	::SSL_get0_alpn_selected(pSSL_, &data, &len);
	if (!data) {
		return ALPN_HTTP_V1_1;
	}
	return fromString(std::string(reinterpret_cast<char const *>(data), len));
}

OpenSslEngine::~OpenSslEngine() {
	if (!pSSL_) {
		return;
	}
	::SSL_set_shutdown(pSSL_, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
	::SSL_free(pSSL_);
}

void OpenSslEngine::shutdown() {
	auto ret = ::SSL_shutdown(pSSL_);
	if (ret == 0) {
		::SSL_shutdown(pSSL_);
	}
}

void OpenSslEngine::reset() {
	isNegotiationFinished_ = false;
	SSL_set_app_data(pSSL_, this);
	::SSL_set_accept_state(pSSL_);
}

bool OpenSslEngine::decrypt(rapid::IoBuffer *pBuffer) {
	RAPID_LOG_TRACE_FUNC();

	RAPID_ENSURE(!pBuffer->isEmpty());

	while (true) {
		auto bytesWrite = ::BIO_write(pInputBIO_, pBuffer->peek(), pBuffer->readable());
		pBuffer->retrieve(bytesWrite);

		while (true) {
			auto bytesRead = ::SSL_read(pSSL_, pBuffer->writeData(), pBuffer->writeable());
			if (bytesRead > 0) {
				pBuffer->advanceWriteIndex(bytesRead);
				continue;
			}
			else if (bytesRead == 0) {
				shutdown();
				return true;
			}

			auto error = ::SSL_get_error(pSSL_, bytesRead);
			if (error == SSL_ERROR_WANT_READ) {
				return true;
			}
			else {
				throw SSLException(pSSL_, bytesRead);
			}
		}
	}
	return false;
}

bool OpenSslEngine::handshake(rapid::IoBuffer *pSource, rapid::IoBuffer *pDest) {
	RAPID_LOG_TRACE_FUNC();

	if (isNegotiationFinished_) {
		return true;
	}

	for (;;) {
		auto status = ::SSL_do_handshake(pSSL_);
		if (status == 1) {			
			// Server send 'Change Cipher Spec Protocol: Change Cipher Spec'
			if (writeFromBuffer(pDest)) {
				isNegotiationFinished_ = true;
				return true;
			}
			return false;
		}

		if (!writeFromBuffer(pDest)) {
			return false;
		}

		auto error = ::SSL_get_error(pSSL_, status);
		switch (error) {
		case SSL_ERROR_WANT_READ: {
				if (pSource->isEmpty()) {
					return false;
				}
				auto bytesWrite = ::BIO_write(pInputBIO_, pSource->peek(), pSource->readable());
				pSource->retrieve(bytesWrite);
			}								  
			break;
		default:
			throw SSLException(pSSL_, status);
			break;
		}
	}	
	return false;
}

void OpenSslEngine::encrypt(rapid::IoBuffer *pBuffer) {
	RAPID_LOG_TRACE_FUNC();

	while (!pBuffer->isEmpty()) {
		auto bytesWrite = ::SSL_write(pSSL_, pBuffer->peek(), pBuffer->readable());

		auto status = ::SSL_get_error(pSSL_, bytesWrite);
		switch (status) {
		case SSL_ERROR_NONE:
			pBuffer->retrieve(bytesWrite);
			break;
		default:
			throw SSLException(pSSL_, bytesWrite);
			break;
		}
	}

	while (!writeFromBuffer(pBuffer)) {}
}

bool OpenSslEngine::writeFromBuffer(rapid::IoBuffer *pBuffer) {
	auto bytesPendding = ::BIO_ctrl_pending(pOutputBIO_);
	if (!bytesPendding) {
		return true;
	}
	if (bytesPendding > pBuffer->writeable()) {
		pBuffer->makeWriteableSpace(bytesPendding);
	}
	auto bytesRead = ::BIO_read(pOutputBIO_, pBuffer->writeData(), bytesPendding);
	pBuffer->advanceWriteIndex(bytesRead);
	return false;
}
