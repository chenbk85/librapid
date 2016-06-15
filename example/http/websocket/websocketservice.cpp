#include <unordered_map>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <rapid/iobuffer.h>
#include <rapid/platform/performancecounter.h>
#include <rapid/platform/utils.h>
#include <rapid/logging/logging.h>
#include <rapid/platform/spinlock.h>
#include <rapid/details/timingwheel.h>
#include <rapid/utils/stringutilis.h>

#include "websocketservice.h"

class WebSocketPerfmonService : public WebSocketService {
public:
	WebSocketPerfmonService() {
		pTimer_ = rapid::details::Timer::createTimer();
		auto appName = rapid::platform::getApplicationFileName();
		counters_.push_back(std::make_unique<rapid::platform::PerformanceCounter>(L"Process", L"% Processor Time", appName));
		counters_.push_back(std::make_unique<rapid::platform::PerformanceCounter>(L"Process", L"Pool Nonpaged Bytes", appName));
		counters_.push_back(std::make_unique<rapid::platform::PerformanceCounter>(L"Process", L"Page Faults/sec", appName));
		counters_.push_back(std::make_unique<rapid::platform::PerformanceCounter>(L"Process", L"Working Set", appName));
	}

	virtual ~WebSocketPerfmonService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		// Set dummy send handler, We don't care about send success or not!
		pConn->setSendEventHandler([](rapid::ConnectionPtr) {});	

		pTimer_->start([pConn, this]() {
			auto pBuffer = pConn->getSendBuffer();
			if (!pBuffer->isCompleted()) {
				return;
			}

			try {
				writeToJSON(pBuffer);
				pConn->sendAsync();
			} catch (rapid::Exception const &e) {
				RAPID_LOG_ERROR() << e.what();
				pConn->sendAndDisconnec();
			}
		}, 1000);
	}

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		return false;
	}

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) override {
	}

private:
	void writeToJSON(rapid::IoBuffer *pBuffer) {
		WebSocketResponse response;

		rapidjson::Document doc(rapidjson::Type::kObjectType);
		rapidjson::Value host(rapidjson::Type::kObjectType);

		host.SetString("Server");
		doc.AddMember("host", host, doc.GetAllocator());

		rapidjson::Value time(rapidjson::Type::kObjectType);
		auto currentTime = std::to_string(std::time(nullptr));
		time.SetString(currentTime.c_str(), currentTime.length());
		doc.AddMember("time", time, doc.GetAllocator());

		rapidjson::Value counters(rapidjson::Type::kObjectType);

		for (auto const &counter : counters_) {
			rapidjson::Value counterValue(rapidjson::Type::kObjectType);
			auto current = std::to_string(counter->currentAsLong());
			counterValue.SetString(current.c_str(), current.length(), doc.GetAllocator());

			auto temp = rapid::utils::formUtf16(counter->path());
			rapidjson::Value counterPathName(rapidjson::Type::kStringType);
			counterPathName.SetString(temp.c_str(), temp.length(), doc.GetAllocator());
			counters.AddMember(counterPathName, counterValue, doc.GetAllocator());
		}

		doc.AddMember("counters", counters, doc.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		response.setContentLength(buffer.GetSize());
		response.serialize(pBuffer);
		pBuffer->append(buffer.GetString(), buffer.GetSize());
	}

	rapid::details::TimerPtr pTimer_;
	std::vector<std::unique_ptr<rapid::platform::PerformanceCounter>> counters_;
};

static std::string const MSG_TYPE_ID = "id";
static std::string const MSG_TYPE_MESSAGE = "message";
static std::string const MSG_TYPE_USERNAME = "username";
static std::string const MSG_TYPE_USERLIST = "userlist";

static rapid::platform::Spinlock s_lock;
static std::unordered_map<std::string, rapid::ConnectionPtr> s_userlist;

class WebSocketChatService : public WebSocketService {
public:
	struct Message {
		std::string types;
		std::string id;
		int64_t date;
		std::string content;
	};

	WebSocketChatService() {
		messages_.reserve(16);
	}

	virtual ~WebSocketChatService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		pConn->setSendEventHandler([this, pContext](rapid::ConnectionPtr &conn) {
			pContext->readLoop(conn);
		});

		writeUid(time(nullptr), pConn->getSendBuffer());
		pConn->sendAsync();
	}

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		bool isSendCompleted = true;

		if (webSocketRequest->isClosed()) {
			onWebSocketClose(pConn);
			return false; // ¤£Ä~ÄòÅª¨ú!
		}

		auto pBuffer = pConn->getReceiveBuffer();
		if (pBuffer->isEmpty()) {
			return true;
		}

		auto message = parse(pBuffer);

		if (message.types == MSG_TYPE_USERNAME) {
			std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
			s_userlist[message.content] = pConn;
			
			username_ = message.content;
			enqueueUsernameResponse(username_);
			enqueueUserListResponse(toUserList());
		} else if (message.types == MSG_TYPE_MESSAGE) {
			enqueueMessageResponse(username_, message.content);
		}

		isSendCompleted = broadcastMessage();
		return isSendCompleted;
	}

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) override {
		{
			std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
			s_userlist.erase(username_);
		}
		enqueueUserListResponse(toUserList());
		broadcastMessage();
	}

private:
	void AddMember(rapidjson::Document &doc, std::string const &key, std::string const &value) {
		rapidjson::Value object(rapidjson::Type::kObjectType);
		object.SetString(value.c_str(), doc.GetAllocator());

		rapidjson::Value keyobject(rapidjson::Type::kStringType);
		keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
		doc.AddMember(keyobject, object, doc.GetAllocator());
	}

	void AddMember(rapidjson::Document &doc, std::string const &key, int64_t const &value) {
		rapidjson::Value object(rapidjson::Type::kObjectType);
		object.SetInt64(value);

		rapidjson::Value keyobject(rapidjson::Type::kStringType);
		keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
		doc.AddMember(keyobject, object, doc.GetAllocator());
	}

	void writeUid(int uid, rapid::IoBuffer *pBuffer) {
		rapidjson::Document doc(rapidjson::Type::kObjectType);

		AddMember(doc, "type", MSG_TYPE_ID);
		AddMember(doc, "id", std::to_string(uid));
		AddMember(doc, "date", time(nullptr));

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		WebSocketResponse response;
		response.setContentLength(buffer.GetSize());
		response.serialize(pBuffer);
		pBuffer->append(buffer.GetString(), buffer.GetSize());
	}

	void enqueueUsernameResponse(std::string const &username) {
		rapidjson::Document doc(rapidjson::Type::kObjectType);

		AddMember(doc, "type", MSG_TYPE_USERNAME);
		AddMember(doc, "name", username);
		AddMember(doc, "date", time(nullptr));

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		messages_.emplace_back(buffer.GetString(), buffer.GetSize());
	}

	void enqueueMessageResponse(std::string const &username, std::string const &text) {
		rapidjson::Document doc(rapidjson::Type::kObjectType);

		AddMember(doc, "type", MSG_TYPE_MESSAGE);
		AddMember(doc, "name", username);
		AddMember(doc, "date", time(nullptr));
		AddMember(doc, "text", text);

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		messages_.emplace_back(buffer.GetString(), buffer.GetSize());
	}

	void enqueueUserListResponse(std::vector<std::string> const &userlist) {
		rapidjson::Document doc(rapidjson::Type::kObjectType);

		AddMember(doc, "type", MSG_TYPE_USERLIST);

		rapidjson::Value array(rapidjson::kArrayType);

		for (auto const &user : userlist) {
			rapidjson::Value object(rapidjson::kObjectType);
			object.SetString(user.c_str(), user.length());
			array.PushBack(object, doc.GetAllocator());
		}

		doc.AddMember("users", array, doc.GetAllocator());
		
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		messages_.emplace_back(buffer.GetString(), buffer.GetSize());
	}

	std::vector<std::string> toUserList() {
		std::vector<std::string> userlist;
		userlist.reserve(s_userlist.size());
		for (auto &pair : s_userlist) {
			userlist.push_back(pair.first);
		}
		return userlist;
	}

	Message parse(rapid::IoBuffer *pBuffer) {
		auto data = pBuffer->read(pBuffer->readable());

		RAPID_LOG_TRACE() << "Chat Debug (" << data.length() << ")\r\n" << data;

		rapidjson::Document doc;
		doc.Parse(data.c_str());
		if (doc.HasParseError()) {
			throw rapid::Exception("Parse json 45error");
		}

		auto id = doc.FindMember("id");
		auto type = doc.FindMember("type");
		auto date = doc.FindMember("date");
		std::string content;

		if ((*type).value.GetString() == MSG_TYPE_USERNAME) {
			auto name = doc.FindMember("name");
			content = (*name).value.GetString();
		} else if ((*type).value.GetString() == MSG_TYPE_MESSAGE) {
			auto text = doc.FindMember("text");
			content = (*text).value.GetString();
		}

		return {
			(*type).value.GetString(),
			(*id).value.GetString(),
			(*date).value.GetInt64(),
			content,
		};
	}
	
	bool broadcastMessage() {
		std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
		
		while (!messages_.empty()) {
			auto message = messages_.back();

			for (auto &user : s_userlist) {
				auto pBuffer = user.second->getSendBuffer();
				WebSocketResponse response;
				response.setContentLength(message.length());
				response.serialize(pBuffer);
				pBuffer->append(message.c_str(), message.length());
			}
			messages_.pop_back();
		}

		auto isSendCompleted = false;

		for (auto &pair : s_userlist) {
			if (pair.first == username_) {
				isSendCompleted = pair.second->sendAsync();
			} else {
				pair.second->sendAsync();
			}
		}
		return isSendCompleted;
	}
	
	std::vector<std::string> messages_;
	std::string username_;
};

class WebSocketEchoService : public WebSocketService {
public:
	WebSocketEchoService() {

	}

	virtual ~WebSocketEchoService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		pContext->readLoop(pConn);
	}

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		bool isSendCompleted = true;

		if (!webSocketRequest->isClosed()) {
			auto pReceiveBuffer = pConn->getReceiveBuffer();
			auto content = pReceiveBuffer->read(pReceiveBuffer->readable());
			WebSocketResponse response;
			auto pSendBuffer = pConn->getSendBuffer();
			response.setContentLength(content.length());
			response.serialize(pSendBuffer);
			pSendBuffer->append(content);
			isSendCompleted = pConn->sendAsync();
		} else {
			onWebSocketClose(pConn);
		}

		return isSendCompleted;
	}

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) override {

	}
};

class WebSocketBenchmarkService : public WebSocketService {
public:
	WebSocketBenchmarkService() {

	}

	virtual ~WebSocketBenchmarkService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		pContext->readLoop(pConn);
	}

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		bool isSendCompleted = true;
		char buffer[1024] = { '1', '0', '2', '4', 0 };

		if (!webSocketRequest->isClosed()) {
			auto pReceiveBuffer = pConn->getReceiveBuffer();
			auto content = pReceiveBuffer->read(pReceiveBuffer->readable());
			WebSocketResponse response;
			auto pSendBuffer = pConn->getSendBuffer();
			response.setContentLength(sizeof(buffer));
			response.serialize(pSendBuffer);
			pSendBuffer->append(buffer, sizeof(buffer));
			isSendCompleted = pConn->sendAsync();
		} else {
			onWebSocketClose(pConn);
		}

		return isSendCompleted;
	}

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) override {

	}
};


WebSocketService::WebSocketService() {
}

std::shared_ptr<WebSocketService> WebSocketService::createService(std::string const & protocol) {
	if (protocol == "chat") {
		return std::make_shared<WebSocketChatService>();
	} else if (protocol == "perfmon") {
		return std::make_shared<WebSocketPerfmonService>();
	}
	//return std::make_shared<WebSocketEchoService>();
	return std::make_shared<WebSocketBenchmarkService>();
}
