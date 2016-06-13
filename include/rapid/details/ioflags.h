//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>

namespace rapid {

namespace details {

struct IOFlags {
	enum Flags : uint8_t {
		IO_ACCEPT_COMPLETED,
		IO_ACCEPT_PENDDING,
		IO_SEND_FILE_COMPLETED,
		IO_SEND_FILE_PENDDING,
		IO_SEND_COMPLETED,
		IO_SEND_PENDDING,
		IO_RECV_COMPLETED,
		IO_RECV_PENDDING,
		IO_DISCONNECT_COMPLETED,
		IO_DISCONNECT_PENDDING,
		IO_POST_PENDDING,
		IO_POST_COMPLETED,
		_MAX_IO_OPT_FLAG_
	};

	IOFlags() noexcept
		: flags(_MAX_IO_OPT_FLAG_) {
	}

	IOFlags(Flags f) noexcept
		: flags(f) {
	}

	bool operator!=(Flags f) const noexcept {
		return flags != f;
	}

	bool operator==(Flags f) const noexcept {
		return flags == f;
	}

	friend std::ostream& operator<< (std::ostream& ostr, IOFlags const &ioFlags) {
#define OUTPUT_CASE_STR(caseName) \
	case IOFlags::caseName: ostr << #caseName; break

		switch (ioFlags.flags) {
			OUTPUT_CASE_STR(_MAX_IO_OPT_FLAG_);
			OUTPUT_CASE_STR(IO_ACCEPT_COMPLETED);
			OUTPUT_CASE_STR(IO_ACCEPT_PENDDING);
			OUTPUT_CASE_STR(IO_SEND_FILE_PENDDING);
			OUTPUT_CASE_STR(IO_SEND_FILE_COMPLETED);
			OUTPUT_CASE_STR(IO_SEND_COMPLETED);
			OUTPUT_CASE_STR(IO_SEND_PENDDING);
			OUTPUT_CASE_STR(IO_RECV_COMPLETED);
			OUTPUT_CASE_STR(IO_RECV_PENDDING);
			OUTPUT_CASE_STR(IO_DISCONNECT_COMPLETED);
			OUTPUT_CASE_STR(IO_DISCONNECT_PENDDING);
			OUTPUT_CASE_STR(IO_POST_PENDDING);
			OUTPUT_CASE_STR(IO_POST_COMPLETED);
			break;
		}
		return ostr;
	}

	Flags flags;
};

}

}

