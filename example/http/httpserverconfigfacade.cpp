//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <vector>
#include <functional>
#include <unordered_map>

#include <rapid/logging/logging.h>
#include <rapid/logging/utilis.h>

#include <rapid/utils/stringutilis.h>

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>

#include "httpserverconfigfacade.h"

static rapid::logging::Level getLogLevel(std::string const &str) {
	static std::unordered_map<std::string, rapid::logging::Level> const levelTables{
		{ "Info",  rapid::logging::Info },
		{ "Error",  rapid::logging::Error },
		{ "Fatal",  rapid::logging::Fatal },
		{ "Warn",  rapid::logging::Warn },
		{ "Trace",  rapid::logging::Trace },
	};
	auto itr = levelTables.find(str);
	return (*itr).second;
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
		auto pFileLogAppender = std::make_shared<rapid::logging::FileLogAppender>();
		pFileLogAppender->setLogDirectory(rapid::utils::fromBytes(sets["LogDirectory"]));
		rapid::logging::addLogAppender(pFileLogAppender);
	} 
	if (settings.find("ConsoleAppender") != settings.end()) {
		auto consoleAppender = settings.find("ConsoleAppender");
		readXmlSettings((*consoleAppender).second, sets);
		auto pConsoleOuputAppender = std::make_shared<rapid::logging::ConsoleOutputLogAppender>();
		pConsoleOuputAppender->setConsoleFont(rapid::utils::fromBytes(sets["Font"]),
			std::strtoul(sets["FontSize"].c_str(), nullptr, 10));
		pConsoleOuputAppender->setWindowSize(80, 50);
		rapid::logging::addLogAppender(pConsoleOuputAppender);
	}
}

HttpServerConfigFacade::HttpServerConfigFacade()
	: enableHttp2Proto_(false)
	, enableSSLProto_(false)
	, listenPort_(80)
	, upgradeableHttp2_(false)
	, numaNode_(0)
	, bufferSize_(0)
	, maxUserConnection_(0)
	, initialUserConnection_(0) {
}

HttpServerConfigFacade::~HttpServerConfigFacade() {
	rapid::logging::stopLogging();
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
	if (!ssl) {
		readXmlSettings(ssl, sslSettings);
		// Setting Http config
		{
			certificateFilePath_ = sslSettings["CertificateFilePath"];
			privateKeyFilePath_ = sslSettings["PrivateKeyFilePath"];
		}
	}

	std::map<std::string, std::string> loggerSettings;
	auto logger = (*httpServer).first_node("Logger");
	readXmlSettings(logger, loggerSettings);

	rapid::logging::startLogging(getLogLevel(loggerSettings["Level"]));

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


