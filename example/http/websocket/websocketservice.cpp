#include <unordered_map>
#include <vector>
#include <chrono>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <rapid/iobuffer.h>
#include <rapid/platform/performancecounter.h>
#include <rapid/platform/utils.h>
#include <rapid/logging/logging.h>
#include <rapid/logging/mpmc_bounded_queue.h>
#include <rapid/platform/spinlock.h>
#include <rapid/details/timingwheel.h>
#include <rapid/utils/stopwatch.h>
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
static uint32_t constexpr MSG_MAX_SIZE = 4096;

class WebSocketServerPusher : public rapid::utils::Singleton<WebSocketServerPusher> {
public:
	WebSocketServerPusher()
		: messages_(MSG_MAX_SIZE)
		, pumpThread_([this]() { messageListenerLoop(); }) {
	}

	void subscribe(int id, rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) {
		std::lock_guard<rapid::platform::Spinlock> guard{
			lock_
		};

		userlist_[id] = std::make_pair(pConn, pContext);
	}

	void unSubscribe(int id) {
		std::lock_guard<rapid::platform::Spinlock> guard{
			lock_
		};

		usernames_.erase(id);
		userlist_.erase(id);
	}

	void setUsername(int uid, std::string const &username) {
		std::lock_guard<rapid::platform::Spinlock> guard{
			lock_
		};

		usernames_[uid] = username;
	}

	std::vector<std::string> getUserlist() {
		std::lock_guard<rapid::platform::Spinlock> guard{
			lock_
		};

		std::vector<std::string> userlist;
		userlist.reserve(usernames_.size());

		for (auto &pair : usernames_)
			userlist.push_back(pair.second);

		return userlist;
	}

	void submitMessage(std::vector<std::string> &messages) {
		messages_.enqueue(std::move(messages));
	}

	void sendMessage(rapid::ConnectionPtr &pConn, std::vector<std::string> const &messages) {
		auto pBuffer = pConn->getSendBuffer();

		for (auto const &message : messages) {
			WebSocketResponse response;
			response.setContentLength(message.length());
			response.serialize(pBuffer);
			pBuffer->append(message.c_str(), message.length());
		}

		pConn->sendAsync();
	}

	void messageListenerLoop() {
		updateTime_.reset();

		while (true) {
			std::vector<std::string> messages;

			if (!messages_.dequeue(messages)) {
				auto elapsed = updateTime_.elapsed<std::chrono::microseconds>();
				if (elapsed <= std::chrono::microseconds(50)) {
					continue;
				}
				if (elapsed <= std::chrono::microseconds(100)) {
					std::this_thread::yield();
					continue;
				}
				if (elapsed <= std::chrono::milliseconds(200)) {
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					continue;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				continue;
			}

			updateTime_.reset();

			std::lock_guard<rapid::platform::Spinlock> guard{
				lock_
			};

			for (auto &user : userlist_) {
				auto pBuffer = user.second.first->getSendBuffer();

				if (pBuffer->isCompleted()) {
					sendMessage(user.second.first, std::move(messages));
				} else {
					user.second.first->postAsync([messages, this](rapid::ConnectionPtr &pConn) {
						sendMessage(pConn, messages);
					});
				}
			}
		}
	}

	mutable rapid::platform::Spinlock lock_;
	mpmc_bounded_queue<std::vector<std::string>> messages_;
	std::unordered_map<int, std::string> usernames_;
	std::unordered_map<int, std::pair<rapid::ConnectionPtr, std::shared_ptr<HttpContext>>> userlist_;
	rapid::utils::SystemStopwatch updateTime_;
	std::thread pumpThread_;
};

class WebSocketChatService : public WebSocketService {
public:
	struct Message {
		std::string types;
		std::string id;
		int64_t date;
		std::string content;
	};

	WebSocketChatService() {
	}

	virtual ~WebSocketChatService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		uid_ = time(nullptr);
		writeUid(uid_);

		pConn->setSendEventHandler([this, pContext](rapid::ConnectionPtr &conn) {
			pContext->readLoop(conn);
		});

		WebSocketServerPusher::getInstance().subscribe(uid_, pConn, pContext);
		WebSocketServerPusher::getInstance().submitMessage(messages_);
	}

	virtual bool onWebSocketMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		if (webSocketRequest->isClosed()) {
			onWebSocketClose(pConn);
			return false; // ¤£Ä~ÄòÅª¨ú!
		}

		auto pBuffer = pConn->getReceiveBuffer();
		if (pBuffer->isEmpty())
			return true;

		auto message = parse(pBuffer);

		if (message.types == MSG_TYPE_USERNAME) {			
			username_ = message.content;
			WebSocketServerPusher::getInstance().setUsername(uid_, username_);
			addUsernameMessage(username_);
			addUserListMessage(WebSocketServerPusher::getInstance().getUserlist());
		} else if (message.types == MSG_TYPE_MESSAGE) {
			addChatMessage(username_, message.content);
		}

		WebSocketServerPusher::getInstance().submitMessage(messages_);
		return false;
	}

	virtual void onWebSocketClose(rapid::ConnectionPtr &pConn) override {
		WebSocketServerPusher::getInstance().unSubscribe(uid_);
		addUserListMessage(WebSocketServerPusher::getInstance().getUserlist());
		WebSocketServerPusher::getInstance().submitMessage(messages_);
	}

private:
	void addMember(rapidjson::Document &doc, std::string const &key, std::string const &value) {
		rapidjson::Value object(rapidjson::Type::kObjectType);
		object.SetString(value.c_str(), doc.GetAllocator());

		rapidjson::Value keyobject(rapidjson::Type::kStringType);
		keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
		doc.AddMember(keyobject, object, doc.GetAllocator());
	}

	void addMember(rapidjson::Document &doc, std::string const &key, int64_t const &value) {
		rapidjson::Value object(rapidjson::Type::kObjectType);
		object.SetInt64(value);

		rapidjson::Value keyobject(rapidjson::Type::kStringType);
		keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
		doc.AddMember(keyobject, object, doc.GetAllocator());
	}

	void writeUid(int uid) {
		messages_.push_back(createMessage([this, uid](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_ID);
			addMember(doc, "id", std::to_string(uid));
			addMember(doc, "date", time(nullptr));
		}));
	}

	template <typename Lambda>
	std::string createMessage(Lambda &&addMemberHandler) {
		rapidjson::Document doc(rapidjson::Type::kObjectType);

		addMemberHandler(doc);

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	void addUsernameMessage(std::string const &username) {
		messages_.push_back(createMessage([this, username](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_USERNAME);
			addMember(doc, "name", username);
			addMember(doc, "date", time(nullptr));
		}));
	}

	void addChatMessage(std::string const &username, std::string const &text) {
		messages_.push_back(createMessage([this, username, text](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_MESSAGE);
			addMember(doc, "name", username);
			addMember(doc, "date", time(nullptr));
			addMember(doc, "text", text);
		}));
	}

	void addUserListMessage(std::vector<std::string> const &userlist) {
		messages_.push_back(createMessage([this, userlist](rapidjson::Document &doc) {
			rapidjson::Value array(rapidjson::kArrayType);

			addMember(doc, "type", MSG_TYPE_USERLIST);

			for (auto const &user : userlist) {
				rapidjson::Value object(rapidjson::kObjectType);
				object.SetString(user.c_str(), user.length());
				array.PushBack(object, doc.GetAllocator());
			}

			doc.AddMember("users", array, doc.GetAllocator());
		}));
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
	
	std::vector<std::string> messages_;
	int uid_;
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
