//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>

namespace rapid {

namespace platform {
		
class ApplicationSingleton {
public:
	ApplicationSingleton();

	ApplicationSingleton(ApplicationSingleton const &) = delete;
	ApplicationSingleton& operator=(ApplicationSingleton const &) = delete;

	~ApplicationSingleton();

	bool hasInstanceRunning() const;
private:
	class ApplicationSingletonImpl;
	std::unique_ptr<ApplicationSingletonImpl> pImpl;
};

}

}
