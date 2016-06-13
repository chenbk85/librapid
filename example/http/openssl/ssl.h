//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <rapid/utils/singleton.h>

class SSLInitializer : public rapid::utils::Singleton<SSLInitializer> {
public:
	SSLInitializer();
	~SSLInitializer();

	SSLInitializer(SSLInitializer const &) = delete;
	SSLInitializer& operator=(SSLInitializer const &) = delete;
};
