#include <fstream>
#include <string>

#include <rapid/logging/logging.h>
#include <rapid/exception.h>

#include "httpstaticheadertable.h"

HttpStaticHeaderTable::HttpStaticHeaderTable() {
	std::ifstream file("headers.txt");
	std::string str;

	headerHashTable_.resize(HASH_TABLE_SIZE);

	if (!file.is_open())
		throw rapid::Exception("Not found file (headers.txt)");

	while (file >> str) {
		auto hashValue = HttpStaticHeaderTable::hash(str);
		auto index = hashValue % HASH_TABLE_SIZE;
		headerHashTable_[index].push_back(std::make_pair(hashValue, std::make_unique<std::string>(str)));
	}
}

HttpStaticHeaderTable::~HttpStaticHeaderTable() {
}

std::pair<uint32_t, std::string const*> HttpStaticHeaderTable::getIndexedName(std::string const& name) const {
	auto hashValue = HttpStaticHeaderTable::hash(name);
	return getIndexedName(hashValue, name);
}

std::pair<uint32_t, std::string const*> HttpStaticHeaderTable::getIndexedName(uint32_t hashValue, std::string const& name) const {
	auto index = hashValue % HASH_TABLE_SIZE;
	if (headerHashTable_[index].size() == 1) {
		return std::pair<uint32_t, std::string const*>(hashValue, headerHashTable_[index][0].second.get());
	}
	for (auto const &header : headerHashTable_[index]) {
		if ((*header.second) == name) {
			return std::pair<uint32_t, std::string const*>(hashValue, header.second.get());
		}
	}
	return std::pair<uint32_t, std::string const*>(0, nullptr);
}
