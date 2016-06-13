//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>

#include <rapid/logging/logging.h>
#include <rapid/platform/registry.h>

#include <rapid/platform/tcpipparameters.h>

namespace rapid {

namespace platform {

enum {
	// Seed: https://technet.microsoft.com/en-us/library/cc938196.aspx
	// By default 5000(Max: 65534)
	DEFAULT_MAX_USER_PORT = 5000,

	// Seed: https://technet.microsoft.com/en-us/library/cc938217.aspx
	// TIME-WAIT: 2MSL為240秒(RF793中定義為MSL為2min).,
	DEFAULT_TCP_TIME_WAIT_DELAY = 240,
};

TcpIpParameters::TcpIpParameters() {
	static std::wstring const TCPIP_PARAMETER_REG_PATH {
		L"SYSTEM\\CurrentControlSet\\Services\\TCPIP\\Parameters\\"
	};

	try {
		platform::Registry reg(HKEY_LOCAL_MACHINE, TCPIP_PARAMETER_REG_PATH);
		timeWaitDelay_ = reg.getDword(L"TcpTimedWaitDelay");

		RAPID_LOG_INFO() << "TcpTimedWaitDelay=" << timeWaitDelay_ << " secs";
	} catch (NotFoundException const &) {
		timeWaitDelay_ = DEFAULT_TCP_TIME_WAIT_DELAY;
		RAPID_LOG_WARN() << "Not found 'TcpTimedWaitDelay' parameter!"
			<< " use default " << timeWaitDelay_;
	}

	try {
		platform::Registry reg(HKEY_LOCAL_MACHINE, TCPIP_PARAMETER_REG_PATH);
		maxUserPort_ = reg.getDword(L"MaxUserPort");

		RAPID_LOG_INFO() << "MaxUserPort=" << maxUserPort_;
	} catch (NotFoundException const &) {
		maxUserPort_ = DEFAULT_MAX_USER_PORT;
		RAPID_LOG_WARN() << "Not found 'MaxUserPort' parameter!"
			<< " use default " << maxUserPort_;
	}
}

uint32_t TcpIpParameters::getTcpTimedWaitDelay() const {
	return timeWaitDelay_;
}

uint16_t TcpIpParameters::getMaxUserPort() const {
	return maxUserPort_;
}

TcpIpParameters::~TcpIpParameters() noexcept {
}

}

}
