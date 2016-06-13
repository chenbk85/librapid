//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

namespace rapid {
	
class IniFileReader {
public:
	explicit IniFileReader(std::string const &filePath);

	int getInt(std::string section, std::string key, int defaultValue) const;

	std::string getString(std::string section, std::string key, std::string defaultValue = "") const;

	bool getBoolean(std::string section, std::string key, bool defaultValue = false) const;

	void writeString(std::string section, std::string key, std::string str) const;
private:
	std::string iniFilePath_;
};

}
