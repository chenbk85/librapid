//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <vector>

#include <rapid/platform/platform.h>
#include <rapid/utils/stringutilis.h>
#include <rapid/platform/inifilereader.h>
#include <rapid/exception.h>

namespace rapid {
	
IniFileReader::IniFileReader(std::string const &filePath)
	: iniFilePath_(filePath) {
}

int IniFileReader::getInt(std::string section, std::string key, int defaultValue) const {
	return ::GetPrivateProfileIntA(section.c_str(), key.c_str(), defaultValue, iniFilePath_.c_str());
}

std::string IniFileReader::getString(std::string section, std::string key, std::string defaultValue) const {
	auto bufSize = MAX_PATH;
	auto count = 1;
	auto retval = 0;
	
	std::vector<char> buffer;

	do {
		bufSize *= count;
		buffer.resize(bufSize);

		retval = ::GetPrivateProfileStringA(section.c_str(),
			key.c_str(),
			defaultValue.c_str(),
			buffer.data(),
			buffer.size(),
			iniFilePath_.c_str());

	} while ((retval == ((bufSize * count) - 1))
		|| (retval == ((bufSize * count) - 2)));
	return buffer.data();
}

void IniFileReader::writeString(std::string section, std::string key, std::string str) const {
	if (!::WritePrivateProfileStringA(section.c_str(), key.c_str(), str.c_str(), iniFilePath_.c_str())) {
		throw Exception();
	}
}

bool IniFileReader::getBoolean(std::string section, std::string key, bool defaultValue) const {
	auto retval = getString(section, key, "false");
	return retval == "true";
}

}
