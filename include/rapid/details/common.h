//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <algorithm>

namespace rapid {

namespace details {

inline uint32_t roundUp(uint32_t value, uint32_t boundary) {
    return ((value - 1) | (boundary - 1)) + 1;
}

inline void dumpHex(std::ostream &stream, char const *data, uint32_t len) {
	static uint32_t constexpr OUTPUT_BUFFER_MAX = 33;
	static uint32_t constexpr INPUT_BUFFER_MAX = 16;
	
	auto totalBytes = len;

    for (uint32_t j = 0; j < len;) {
		uint8_t ouput[OUTPUT_BUFFER_MAX] = { 0 };
		uint8_t input[INPUT_BUFFER_MAX] = { 0 };

		auto copyBytes = (std::min)(totalBytes, INPUT_BUFFER_MAX);
		memcpy(input, &data[j], copyBytes);
		
		for (uint32_t i = 0; i < copyBytes; ++i) {
			if (input[i] >= ' ')
                ouput[i] = input[i];
            else
                ouput[i] = '.';

            stream << std::setfill('0')
                   << std::setw(2)
                   << std::hex
                   << static_cast<uint32_t>(input[i])
                   << ' ';
        }
		
		if (copyBytes < INPUT_BUFFER_MAX) {
			for (size_t i = 0; i < INPUT_BUFFER_MAX - copyBytes; ++i) {
				stream << std::setfill(' ') << std::setw(2) << ' ' << ' ';
			}
		}
        
		stream << "  " << ouput << "\r\n";
        j += copyBytes;
        totalBytes -= copyBytes;
    }
}

}

}
