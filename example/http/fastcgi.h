//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include <rapid/utils/byteorder.h>
#include <rapid/iobuffer.h>

enum FastCGIFlags : uint8_t {
	FCGI_RESPONDER = 1,
	FCGI_KEEP_CONN = 1,
	FCGI_BEGIN_REQUEST = 1,
	FCGI_END_REQUEST = 3,
	FCGI_PARAMS,
	FCGI_STDIN,
	FCGI_STDOUT,
	FCGI_STDERR,
	FCGI_DATA,
};

struct FastCGIHeader {
	uint8_t version;
	uint8_t type;
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t paddingLength;
	uint8_t reserved;
};

enum {
	FCGI_VERSION_1 = 1,
	FCGI_BEGIN_REQUEST_BODY_SIZE = 8,
	FCGI_RECORD_HEADER_SIZE = sizeof(FastCGIHeader)
};

struct FastCGIBeginRequest {
	uint16_t role;
	uint8_t flag;
	uint8_t reserved[5];
};

void writeFastCGIHeader(rapid::IoBuffer *pBuffer, uint8_t type, uint16_t requestId, uint16_t contentLength) {
	pBuffer->makeWriteableSpace(FCGI_RECORD_HEADER_SIZE);
	FastCGIHeader *header = reinterpret_cast<FastCGIHeader*>(pBuffer->peek());
	header->version = FCGI_VERSION_1;
	rapid::utils::LittleEndian::copyAndSwapTo(&header->requestId, &requestId, sizeof(uint16_t));
	rapid::utils::LittleEndian::copyAndSwapTo(&header->contentLength, &requestId, sizeof(uint16_t));
	header->type = type;
	header->paddingLength = 0;
	header->reserved = 0;
}
