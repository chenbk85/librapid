//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include <rapid/utils/singleton.h>

namespace rapid {

namespace platform {

class TcpIpParameters : public utils::Singleton<TcpIpParameters> {
public:
	TcpIpParameters();
	~TcpIpParameters() noexcept;

	uint32_t getTcpTimedWaitDelay() const;

	uint16_t getMaxUserPort() const;
private:
	uint32_t timeWaitDelay_;
	uint16_t maxUserPort_;
};

}

}
