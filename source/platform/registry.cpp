//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/exception.h>

#include <rapid/details/contracts.h>
#include <rapid/platform/registry.h>

namespace rapid {

namespace platform {

static HKEY getSectionKey(HKEY branch, std::wstring const &section, long permission = KEY_WRITE | KEY_READ) {
	HKEY key = nullptr;
	auto retval = ::RegOpenKeyExW(branch, section.c_str(), 0, permission, &key);
	if (retval == ERROR_SUCCESS) {
		return key;
	} else if (retval == ERROR_FILE_NOT_FOUND) {
		throw NotFoundException();
	}
	if (key != nullptr) {
		::RegCloseKey(key);
	}
	throw Exception(retval);
	return nullptr;
}

static HKEY createSectionKey(HKEY branch, std::wstring const &section) {
	HKEY key = nullptr;
	DWORD disposition;
	::RegCreateKeyExW(branch,
		section.c_str(),
		0,
		REG_NONE,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE | KEY_READ,
		nullptr,
		&key,
		&disposition);
	return key;
}

Registry::Registry()
    : baseKey_(nullptr) {
}

Registry::Registry(HKEY key, std::wstring const &section) {
    setRegistryBranch(key, section);
}

void Registry::setRegistryBranch(HKEY key, std::wstring const &section) {
    baseKey_ = key;
	section_ = section;
}

void Registry::setDword(std::wstring const& entry, int value) {
	setDword(baseKey_, section_, entry, value);
}

Registry::~Registry() {
	if (baseKey_ != HKEY_LOCAL_MACHINE) {
		::RegCloseKey(baseKey_);
	}
}

std::unique_ptr<Registry> Registry::createLocalMachineBranch(std::wstring const& section) {
	return std::make_unique<Registry>(createSectionKey(HKEY_LOCAL_MACHINE, section), section);
}

void Registry::deleteLocalMachineBranch(std::wstring const& section) {
	::RegDeleteKey(HKEY_LOCAL_MACHINE, section.c_str());
}

void Registry::setString(HKEY branch,
	std::wstring const& section,
	std::wstring const& entry,
	std::wstring const& value) {
	auto result = ::RegSetValueEx(branch, entry.c_str(), 0, REG_SZ,
		LPBYTE(value.c_str()), value.length() * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		throw Exception();
	}
}

void Registry::setDword(HKEY branch, std::wstring const& section, std::wstring const& entry, int value) {
	auto result = ::RegSetValueEx(branch, entry.c_str(), 0, REG_DWORD, LPBYTE(&value), sizeof(value));
	if (result != ERROR_SUCCESS) {
		throw Exception();
	}
}

uint32_t Registry::getDword(std::wstring const &entry) {
	return getDword(baseKey_, section_, entry);
}

void Registry::setString(std::wstring const& entry, std::wstring const& value) {
	setString(baseKey_, section_, entry, value);
}

std::wstring Registry::getString(std::wstring const &entry, std::wstring const &strDefault) {
	return getString(baseKey_, section_, entry, strDefault);
}

std::vector<BYTE> Registry::getBinary(HKEY branch, std::wstring const &section) {
	std::vector<BYTE> data;

	DWORD size = 0;
	DWORD type = 0;

	// 因為無法測試需要多大的大小, 使用一個較大的大小buffer
	// auto result = ::RegQueryValueEx(branch, section.c_str(), 0, &type, nullptr, &size);
	// data.resize(size);

	size = 1024 * 1024 * 2;
	data.resize(size);

	DWORD result = ERROR_MORE_DATA;
	do {
		result = ::RegQueryValueEx(branch, section.c_str(), 0, &type, data.data(), &size);
		if (result == ERROR_SUCCESS) {
			break;
		}
		else if (result != ERROR_MORE_DATA) {
			throw Exception();
		}
		size *= 2;
		data.resize(size);
	} while (result == ERROR_MORE_DATA);

	// 移除多餘資料
	data.resize(size);
	data.shrink_to_fit();
	return data;
}

std::wstring Registry::getString(HKEY branch, std::wstring const &section,
                                 std::wstring const &entry, std::wstring const &strDefault) {
    auto key = getSectionKey(branch, section.c_str(), KEY_READ);
    if (!key) {
        return L"";
    }
    DWORD type;
    DWORD count;
    auto retval = ::RegQueryValueEx(key, entry.c_str(), nullptr, &type, nullptr, &count);
    if (retval == ERROR_SUCCESS) {
        RAPID_ENSURE(type == REG_SZ || type == REG_EXPAND_SZ);
        std::vector<wchar_t> buffer(count / sizeof(wchar_t));
        retval = ::RegQueryValueEx(key,
                                   entry.c_str(),
                                   nullptr,
                                   &type,
                                   reinterpret_cast<LPBYTE>(buffer.data()),
                                   &count);
    }
    ::RegCloseKey(key);
    return strDefault;
}

uint32_t Registry::getDword(HKEY branch, std::wstring const &section, std::wstring const &entry) {
    auto sectionKey = getSectionKey(branch, section, KEY_READ);
    uint32_t dwValue;
    DWORD dwType;
    DWORD dwCount = sizeof(uint32_t);
    auto retval = ::RegQueryValueExW(sectionKey,
                                     entry.c_str(),
                                     nullptr,
                                     &dwType,
                                     reinterpret_cast<LPBYTE>(&dwValue),
                                     &dwCount);
    ::RegCloseKey(sectionKey);
    if (retval == ERROR_FILE_NOT_FOUND) {
        throw NotFoundException();
    }
    if (retval != ERROR_SUCCESS) {
        throw Exception();
    }
    return dwValue;
}

}

}
