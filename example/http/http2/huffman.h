//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <stdexcept>

#include <rapid/iobuffer.h>

class HuffmanDecodeFailException : std::exception {
public:
	HuffmanDecodeFailException()
		: std::exception("Huffman decode fail") {	
	}
};

class HuffmanEncodeFailException : std::exception {
public:
	HuffmanEncodeFailException()
		: std::exception("Huffman encode fail") {
	}
};

void decodeHuffman(rapid::IoBuffer* buffer, size_t length, std::string &result);

void encodeHuffman(rapid::IoBuffer* buffer, uint8_t const *src, size_t srcLen);