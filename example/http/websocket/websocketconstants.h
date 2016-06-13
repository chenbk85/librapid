//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

uint8_t const WS_MASK_FIN{ 0x80 };
uint8_t const WS_MASK_OPCODE{ 0x0F };
uint8_t const WS_MASK_BIT{ 0x80 };
uint8_t const WS_MASK_PAYLOAD_LENGTH{ 0x7F };

uint8_t const WS_RESERVED_1BIT{ 0x40 };
uint8_t const WS_RESERVED_2BIT{ 0x20 };
uint8_t const WS_RESERVED_3BIT{ 0x10 };

uint8_t const WS_NO_EXT_PAYLOAD_LENGTH_MAX{ 125 };
uint8_t const WS_16BIT_EXT_PAYLOAD_LENGTH{ 126 };
uint8_t const WS_64BIT_EXT_PAYLOAD_LENGTH{ 127 };

uint8_t const WS_MIN_SIZE{ 2 };
uint8_t const WS_MASK_SIZE{ 4 };

std::string const WS_GUID = { "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" };

std::string const WS_MESSAGE = { "WS" };

