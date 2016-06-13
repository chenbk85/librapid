//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <fstream>
#include <vector>
#include <functional>
#include <set>
#include <algorithm>
#include <algorithm>

#include <rapid/logging/logging.h>

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>

#include <rapid/platform/inifilereader.h>

#include "httpserverconfigfacade.h"

HttpServerConfigFacade::HttpServerConfigFacade()
	: listenPort_(80)
	, enableHttp2Proto_(false)
	, enableSSLProto_(false)
	, maxUserConnection_(0)
	, numaNode_(0)
	, bufferSize_(0)
	, initialUserConnection_(0) {
}

static void readXmlSettings(rapidxml::xml_node<char> *node, std::map<std::string, std::string>& settings) {
	for (auto set = (*node).first_node("Set"); set;) {
		std::string name = (*set).first_attribute("name")->value();
		std::string value = set->value();
		settings.insert(std::make_pair(name, value));
		set = (*set).next_sibling("Set");
	}
}

static void readAppenderSettring(rapidxml::xml_node<char> *node, std::map<std::string, rapidxml::xml_node<char> *> &settings) {
	for (auto set = (*node).first_node("Appender"); set;) {
		std::string name = (*set).first_attribute("name")->value();
		settings[name] = set;
		set = (*set).next_sibling("Appender");
	}
}

static void createAppenderFromSetting(std::map<std::string, rapidxml::xml_node<char> *> &settings,
	std::vector<std::shared_ptr<rapid::logging::LogAppender>> &appenders) {
	std::map<std::string, std::string> sets;

	if (settings.find("FileAppender") != settings.end()) {
		auto fileAppender = settings.find("FileAppender");
		readXmlSettings((*fileAppender).second, sets);
	} else if (settings.find("ConsoleAppender") != settings.end()) {
		auto consoleAppender = settings.find("ConsoleAppender");
		readXmlSettings((*consoleAppender).second, sets);
	}
}

void HttpServerConfigFacade::loadIniConfigFile(std::string const &filePath) {
	rapid::IniFileReader reader(filePath);

	// Server
	serverName_ = reader.getString("Server", "ServerName");
	listenPort_ = reader.getInt("Server", "Port", 80);
	bufferSize_ = reader.getInt("Server", "BufferSize", SIZE_16KB);
	initialUserConnection_ = reader.getInt("Server", "InitialUserConnection", 1000);
	maxUserConnection_ = reader.getInt("Server", "MaxUserConnection", 25000);

	// HTTP
	host_ = reader.getString("HTTP", "Host");
	enableSSLProto_ = reader.getBoolean("HTTP", "EnableSSL");
	enableHttp2Proto_ = reader.getBoolean("HTTP", "EnableHttp2");
	tempFilePath_ = reader.getString("HTTP", "TempFilePath");
	rootPath_ = reader.getString("HTTP", "RootPath");
	indexFileName_ = reader.getString("HTTP", "IndexFileName");

	// SSL
	if (enableSSLProto_) {
		// SSL Certificate File Path
		certificateFilePath_ = reader.getString("SSL", "CertificateFilePath");
		privateKeyFilePath_ = reader.getString("SSL", "PrivateKeyFilePath");
	}
}

void HttpServerConfigFacade::loadXmlConfigFile(std::string const &filePath) {
	rapidxml::file<> xmlFile(filePath.c_str());
	rapidxml::xml_document<> doc;
	doc.parse<0>(xmlFile.data());

	auto httpServer = doc.first_node("HttpServer");

	std::map<std::string, std::string> tcpSettings;
	auto server = (*httpServer).first_node("Server");
	readXmlSettings(server, tcpSettings);
	// Setting Tcp config
	{
		listenPort_ = std::strtoul(tcpSettings["Port"].c_str(), nullptr, 10);
		initialUserConnection_ = std::strtoul(tcpSettings["InitialUserConnection"].c_str(), nullptr, 10);
		maxUserConnection_ = std::strtoul(tcpSettings["MaxUserConnection"].c_str(), nullptr, 10);
		bufferSize_ = SIZE_64KB * 2;
		numaNode_ = std::strtoul(tcpSettings["NumaNode"].c_str(), nullptr, 10);
	}

	std::map<std::string, std::string> httpSettings;
	auto http = (*httpServer).first_node("Http");
	readXmlSettings(http, httpSettings);
	// Setting Http config
	{
		host_ = httpSettings["Host"];
		serverName_ = httpSettings["ServerName"];
		enableSSLProto_ = httpSettings["EnableSSL"] == "true" ? true : false;
		enableHttp2Proto_ = httpSettings["EnableHTTP2"] == "true" ? true : false;
		tempFilePath_ = httpSettings["TempFilePath"];
		rootPath_ = httpSettings["RootPath"];
		indexFileName_ = httpSettings["IndexFileName"];
	}

	std::map<std::string, std::string> sslSettings;
	auto ssl = (*httpServer).first_node("SSL");
	readXmlSettings(ssl, sslSettings);
	// Setting Http config
	{
		certificateFilePath_ = sslSettings["CertificateFilePath"];
		privateKeyFilePath_ = sslSettings["PrivateKeyFilePath"];
	}

	std::map<std::string, std::string> loggerSettings;
	auto logger = (*httpServer).first_node("Logger");
	readXmlSettings(logger, loggerSettings);

	std::vector<std::shared_ptr<rapid::logging::LogAppender>> appenders;
	std::map<std::string, rapidxml::xml_node<char> *> settings;
	readAppenderSettring(logger, settings);
	createAppenderFromSetting(settings, appenders);
}

HttpStaticHeaderTable const & HttpServerConfigFacade::getHeadersTable() const {
	return headersTable_;
}

std::string HttpServerConfigFacade::getHost() const {
	return host_;
}

std::string HttpServerConfigFacade::getTempFilePath() const {
	return tempFilePath_;
}

std::string HttpServerConfigFacade::getServerName() const {
	return serverName_;
}

std::string HttpServerConfigFacade::getRootPath() const {
	return rootPath_;
}

std::string HttpServerConfigFacade::getIndexFileName() const {
	return indexFileName_;
}

std::string HttpServerConfigFacade::getPrivateKeyFilePath() const {
	return privateKeyFilePath_;
}

std::string HttpServerConfigFacade::getCertificateFilePath() const {
	return certificateFilePath_;
}


