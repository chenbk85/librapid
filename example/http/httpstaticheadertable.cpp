#include <fstream>
#include <string>

#include "httpstaticheadertable.h"

HttpStaticHeaderTable::HttpStaticHeaderTable() {
	std::ifstream file("headers.txt");
	std::string str;

	headerHashTable_.resize(HASH_TABLE_SIZE);

	while (file >> str) {
		auto hashValue = HttpStaticHeaderTable::hash(str);
		auto index = hashValue % HASH_TABLE_SIZE;
		headerHashTable_[index].push_back(std::make_pair(hashValue, std::make_unique<std::string>(str)));
	}
}

HttpStaticHeaderTable::~HttpStaticHeaderTable() {
}

std::pair<uint32_t, std::string const*> HttpStaticHeaderTable::getIndexedName(std::string const& name) const {
	return getIndexedName(HttpStaticHeaderTable::hash(name));
}

std::pair<uint32_t, std::string const*> HttpStaticHeaderTable::getIndexedName(uint32_t hashValue) const {
	auto index = hashValue % HASH_TABLE_SIZE;
	for (auto const &header : headerHashTable_[index]) {
		if (header.first == hashValue) {
			return std::pair<uint32_t, std::string const*>(hashValue, header.second.get());
		}
	}
	return std::pair<uint32_t, std::string const*>(0, nullptr);
}
