//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <unordered_map>
#include <vector>
#include <chrono>

#include <date.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include <rapid/iobuffer.h>
#include <rapid/platform/performancecounter.h>
#include <rapid/platform/utils.h>
#include <rapid/platform/spinlock.h>
#include <rapid/logging/logging.h>
#include <rapid/utils/mpmc_bounded_queue.h>
#include <rapid/utils/threadpool.h>
#include <rapid/utils/stopwatch.h>
#include <rapid/details/timingwheel.h>
#include <rapid/utils/stringutilis.h>

#include "websocketservice.h"

static std::string const MSG_TYPE_ID = "id";
static std::string const MSG_TYPE_MESSAGE = "message";
static std::string const MSG_TYPE_USERNAME = "username";
static std::string const MSG_TYPE_USERLIST = "userlist";

struct Message {
	static const Message EMPTY_MESSAGE;

	Message()
		: messageId(-1) {
	}

	explicit Message(int id, std::string const &message)
		: messageId(id)
		, content(message) {
	}

	bool operator==(Message const &message) const {
		return messageId == message.messageId;
	}

	int messageId;
	rapid::utils::SystemStopwatch liveTime;
	std::string content;
};

const Message Message::EMPTY_MESSAGE;

static uint32_t makeUniqueUid() {
	static std::atomic<uint32_t> uid = 0;
	return ++uid;
}

static uint32_t makeUniqueMessageId() {
	static std::atomic<uint32_t> messageId = 0;
	return ++messageId;
}

class BroadcastChannel {
public:
	BroadcastChannel(std::string const &name, int maxExpireSecs)
		: maxExpireSecs_(maxExpireSecs)
		, name_(name) {
	}

	std::string name() const {
		return name_;
	}

	void dequeue(std::string const &key) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto ret = messages_.erase(key);
	}

	void enqueue(std::string const &key, std::string const &message) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto id = makeUniqueMessageId();
		if (!key.empty()) {
			messages_[key] = Message(id, message);
		} else {
			messages_[std::to_string(id)] = Message(id, message);
		}
	}

	Message getMessage() const {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		for (auto const & message : messages_) {
			return message.second;
		}
		return Message::EMPTY_MESSAGE;
	}

	Message getMessage(int messageId, int liveTime) const {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		for (auto const & message : messages_) {
			if (message.second.messageId >= messageId) {
				if (maxExpireSecs_ == 0
					|| message.second.liveTime.elapsedCount<std::chrono::seconds>() <= maxExpireSecs_) {
					return message.second;
				}
			}
		}
		return Message::EMPTY_MESSAGE;
	}

private:
	mutable rapid::platform::Spinlock lock_;
	int maxExpireSecs_;
	std::string name_;
	std::unordered_map<std::string, Message> messages_;
};

class BroadcastChannelManager {
public:
	BroadcastChannelManager() {
	}

	virtual ~BroadcastChannelManager() {
	}

	void addChannel(std::string const &channelName, int maxExpireSecs) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		channels_[channelName] = std::make_shared<BroadcastChannel>(channelName, maxExpireSecs);
	}

	void removeChannel(std::string const &channelName) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		channels_.erase(channelName);
	}

	std::shared_ptr<BroadcastChannel> getChannel(std::string const &channelName) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		return channels_[channelName];
	}

	void pushMessage(std::string const &channelName, std::string const &key, std::string const &message) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		onStartupChannel(channelName);
		channels_[channelName]->enqueue(key, message);
		onActiveSubscribers(channelName);
	}

	void pushMessage(std::string const &channelName, int key, std::string const &message) {
		pushMessage(channelName, std::to_string(key), message);
	}

	void removeMessage(std::string const &channelName, int key) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		channels_[channelName]->dequeue(std::to_string(key));
	}

	void removeMessage(std::string const &channelName, std::string const &key) {
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		channels_[channelName]->dequeue(key);
	}

protected:
	virtual void onStartupChannel(std::string const &channelName) {
	}

	virtual void onActiveSubscribers(std::string const &channelName) {
	}

private:
	mutable rapid::platform::Spinlock lock_;
	std::unordered_map<std::string, std::shared_ptr<BroadcastChannel>> channels_;
};

static rapid::platform::Spinlock s_lock;
static std::unordered_map<int, std::string> s_userlist;

static void addUserlist(int id, std::string const &name) {
	std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
	s_userlist[id] = name;
}

static void removeUserlist(int id) {
	std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
	s_userlist.erase(id);
}

static std::vector<int> getUserIdList() {
	std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };

	std::vector<int> userlist;
	for (auto &user : s_userlist) {
		userlist.push_back(user.first);
	}
	return userlist;
}

static std::vector<std::string> getUserlist() {
	std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };

	std::vector<std::string> userlist;
	for (auto &user : s_userlist) {
		userlist.push_back(user.second);
	}
	return userlist;
}

class MessagePusher : public rapid::utils::Singleton<MessagePusher> {
public:
	MessagePusher()
		: stopped_(false)
		, pushThread_([this]() { pushMessageLoop(); }) {
	}

	MessagePusher(MessagePusher const &) = delete;
	MessagePusher& operator=(MessagePusher const &) = delete;

	~MessagePusher() {
		stopped_ = true;
		if (pushThread_.joinable())
			pushThread_.join();
	}

	void subscribe(std::string const &name, rapid::ConnectionPtr &pConn, int maxExpireSecs = INT_MAX) {
		RAPID_TRACE_CALL();
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		connlist_[name] = pConn;
		mananger_.addChannel(name, maxExpireSecs);
	}

	void unSubscribe(std::string const &name) {
		RAPID_TRACE_CALL();
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		connlist_.erase(name);
		mananger_.removeChannel(name);
	}

	void boardcast(std::string const &message) {
		RAPID_TRACE_CALL();
		std::lock_guard<rapid::platform::Spinlock> guard{ lock_ };
		auto subscribeList = getSubscribe();
		for (auto const &subscribe : subscribeList) {
			mananger_.pushMessage(subscribe, "", message);
		}
	}

	void sendMessage(std::string const &name, std::string const &message) {
		RAPID_TRACE_CALL();
		mananger_.pushMessage(name, "", message);
	}

private:
	std::vector<std::string> getSubscribe();

	void pushMessageLoop();

	volatile bool stopped_ : 1;
	mutable rapid::platform::Spinlock lock_;
	std::unordered_map<std::string, rapid::ConnectionPtr> connlist_;
	BroadcastChannelManager mananger_;
	std::thread pushThread_;
};

std::vector<std::string> MessagePusher::getSubscribe() {
	RAPID_TRACE_CALL();
	std::lock_guard<rapid::platform::Spinlock> guard{ s_lock };
	std::vector<std::string> userlist;
	for (auto &user : connlist_) {
		userlist.push_back(user.first);
	}
	return userlist;
}

void MessagePusher::pushMessageLoop() {
	rapid::utils::ThreadPool threadPool;

	while (!stopped_) {
		bool isMessageEmpty = true;

		if (auto lock = rapid::platform::tryToLock(lock_)) {
			for (auto &user : connlist_) {
				auto pBuffer = user.second->getSendBuffer();
				if (!pBuffer->hasCompleted()) {
					break;
				}

				threadPool.startNew([user, pBuffer, this]() {
					auto message = mananger_.getChannel(user.first)->getMessage();
					if (message == Message::EMPTY_MESSAGE) {
						return;
					}

					RAPID_LOG_TRACE() << "Send Message " << message.messageId << std::endl << message.content;
					WebSocketResponse resp;
					resp.setContentLength(message.content.length());
					resp.serialize(pBuffer);
					pBuffer->append(message.content);
					mananger_.removeMessage(user.first, message.messageId);
					user.second->sendAsync();
				});
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

static void addMember(rapidjson::Document &doc, std::string const &key, std::string const &value) {
	rapidjson::Value object(rapidjson::Type::kObjectType);
	object.SetString(value.c_str(), doc.GetAllocator());
	rapidjson::Value keyobject(rapidjson::Type::kStringType);
	keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
	doc.AddMember(keyobject, object, doc.GetAllocator());
}

static void addMember(rapidjson::Document &doc, std::string const &key, int64_t const &value) {
	rapidjson::Value object(rapidjson::Type::kObjectType);
	object.SetInt64(value);
	rapidjson::Value keyobject(rapidjson::Type::kStringType);
	keyobject.SetString(key.c_str(), key.length(), doc.GetAllocator());
	doc.AddMember(keyobject, object, doc.GetAllocator());
}

template <typename Lambda>
static std::string makeMessage(Lambda &&addMemberHandler) {
	rapidjson::Document doc(rapidjson::Type::kObjectType);

	addMemberHandler(doc);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return std::string(buffer.GetString(), buffer.GetSize());
}

struct ChatMessage {
	std::string types;
	std::string id;
	int64_t date;
	std::string content;
};

static ChatMessage parseChatMessage(std::string const &data) {
	RAPID_LOG_TRACE() << "Receive Message" << std::endl << data;

	rapidjson::Document doc;
	doc.Parse(data.c_str());
	if (doc.HasParseError()) {
		throw rapid::Exception("Parse json error");
	}

	// Make sure json format
	auto id = doc.FindMember("id");
	RAPID_ENSURE(id != doc.MemberEnd());

	auto type = doc.FindMember("type");
	RAPID_ENSURE(type != doc.MemberEnd());

	auto date = doc.FindMember("date");
	RAPID_ENSURE(date != doc.MemberEnd());

	std::string content;

	if ((*type).value.GetString() == MSG_TYPE_USERNAME) {
		auto name = doc.FindMember("name");
		RAPID_ENSURE(name != doc.MemberEnd());
		content = (*name).value.GetString();
	} else if ((*type).value.GetString() == MSG_TYPE_MESSAGE) {
		auto text = doc.FindMember("text");
		RAPID_ENSURE(text != doc.MemberEnd());
		content = (*text).value.GetString();
	}

	return{
		(*type).value.GetString(),
		(*id).value.GetString(),
		(*date).value.GetInt64(),
		content,
	};
}

class WebSocketChatService : public WebSocketService {
public:
	WebSocketChatService()
		: uid_(makeUniqueUid()) {
	}

	virtual ~WebSocketChatService() {
	}

	virtual void onOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		MessagePusher::getInstance().subscribe(std::to_string(uid_), pConn);

		pConn->setSendEventHandler([pContext](rapid::ConnectionPtr &conn) {
			auto pBuffer = conn->getReceiveBuffer();
			if (pBuffer->hasCompleted())
				pContext->readLoop(conn);
		});

		addUidMessage(uid_);
	}

	virtual bool onMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		if (webSocketRequest->isClosed()) {
			onClose(pConn);
			return false;
		}

		auto pBuffer = pConn->getReceiveBuffer();
		auto message = parseChatMessage(pBuffer->read(webSocketRequest->contentLength()));

		if (message.types == MSG_TYPE_USERNAME) {
			username_ = message.content;
			addUserlist(uid_, username_);
			addUserLoginMessage(username_);
			updateUserlistMessage(getUserlist());
		} else if (message.types == MSG_TYPE_MESSAGE) {
			addChatRoomMessage(username_, message.content);
		}

		return pBuffer->isEmpty();
	}

	virtual void onClose(rapid::ConnectionPtr &pConn) override {
		MessagePusher::getInstance().unSubscribe(username_);
		removeUserlist(uid_);
		updateUserlistMessage(getUserlist());
	}

private:
	void addUidMessage(int uid) {
		MessagePusher::getInstance().sendMessage(std::to_string(uid_), makeMessage([this, uid](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_ID);
			addMember(doc, "id", std::to_string(uid));
			addMember(doc, "date", time(nullptr));
		}));
	}

	void addUserLoginMessage(std::string const &username) const {
		MessagePusher::getInstance().boardcast(makeMessage([username](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_USERNAME);
			addMember(doc, "name", username);
			addMember(doc, "date", time(nullptr));
		}));
	}

	void addChatRoomMessage(std::string const &username, std::string const &message) const {
		MessagePusher::getInstance().boardcast(makeMessage([username, message](rapidjson::Document &doc) {
			addMember(doc, "type", MSG_TYPE_MESSAGE);
			addMember(doc, "name", username);
			addMember(doc, "date", time(nullptr));
			addMember(doc, "text", message);
		}));
	}

	void updateUserlistMessage(std::vector<std::string> const &userlist) const {
		RAPID_TRACE_CALL();

		MessagePusher::getInstance().boardcast(makeMessage([userlist](rapidjson::Document &doc) {
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

	uint32_t uid_;
	std::string username_;
};

class WebSocketEchoService : public WebSocketService {
public:
	WebSocketEchoService() {

	}

	virtual ~WebSocketEchoService() {

	}

	virtual void onOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		pConn->setSendEventHandler([pContext](rapid::ConnectionPtr &conn) {
			pContext->readLoop(conn);
		});
	}

	virtual bool onMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> pWebSocketRequest) override {
		auto isSendCompleted = false;

		auto pReceiveBuffer = pConn->getReceiveBuffer();

		if (pWebSocketRequest->isClosed()) {
			pReceiveBuffer->retrieve(pWebSocketRequest->contentLength());
			onClose(pConn);
			return true;
		}

		WebSocketResponse resp;
		auto pSendBuffer = pConn->getSendBuffer();
		pReceiveBuffer->retrieve(pWebSocketRequest->contentLength());

		if (!pWebSocketRequest->isTextFormat()) {
			auto content = std::to_string(pWebSocketRequest->contentLength());
			resp.setContentLength(content.length());
			resp.serialize(pSendBuffer);
			pSendBuffer->append(content);
			isSendCompleted = pConn->sendAsync();
		} else {
			isSendCompleted = true;
		}

		return isSendCompleted;
	}

	virtual void onClose(rapid::ConnectionPtr &pConn) override {
		/*
		auto pSendBuffer = pConn->getSendBuffer();
		WebSocketResponse resp;
		resp.setCodec(WebSocketOpcodes::WS_OP_CLOSED);
		resp.setContentLength(0);
		resp.serialize(pSendBuffer);
		pConn->sendAndDisconnec();
		*/
	}
};

class WebSocketPerfmonService : public WebSocketService {
public:
	WebSocketPerfmonService() {
		using namespace rapid::platform;

		pTimer_ = rapid::details::Timer::createTimer();
		uid_ = makeUniqueUid();
		auto appName = getApplicationFileName();

		// CPU counter
		counters_.push_back(std::make_unique<PerformanceCounter>(L"Process", L"% Processor Time", appName));

		// Memory counter
		counters_.push_back(std::make_unique<PerformanceCounter>(L"Process", L"Pool Nonpaged Bytes", appName));
		counters_.push_back(std::make_unique<PerformanceCounter>(L"Process", L"Page Faults/sec", appName));
		counters_.push_back(std::make_unique<PerformanceCounter>(L"Process", L"Working Set", appName));

		// Network counter
		for (auto & interface : getNetworkInterfaceNameList()) {
			// Performance counter 需要將'(', ')'換成'[', ']'
			rapid::utils::replace(interface, "(", "[");
			rapid::utils::replace(interface, ")", "]");
			RAPID_LOG_TRACE() << "Interface: " << interface;
			auto interfaceName = rapid::utils::fromBytes(interface);
			try {
				counters_.push_back(std::make_unique<PerformanceCounter>(
					L"Network Interface", L"Bytes Received/sec",
					interfaceName));
				counters_.push_back(std::make_unique<PerformanceCounter>(
					L"Network Interface", L"Bytes Sent/sec",
					interfaceName));
			} catch (rapid::Exception const &e) {
				RAPID_LOG_IF(rapid::logging::Warn, e.error() != PDH_CSTATUS_NO_INSTANCE) << e.what();
			}
		}
	}

	virtual ~WebSocketPerfmonService() {
	}

	virtual void onOpen(rapid::ConnectionPtr &pConn, std::shared_ptr<HttpContext> pContext) override {
		pusher_.subscribe(std::to_string(uid_), pConn);

		pConn->setSendEventHandler([pContext](rapid::ConnectionPtr &conn) {
			auto pBuffer = conn->getReceiveBuffer();
			if (pBuffer->hasCompleted())
				pContext->readLoop(conn);
		});

		pTimer_ = rapid::details::Timer::createTimer();
		pTimer_->start([this]() {
			pushMessage();
		}, 1000);
	}

	virtual bool onMessage(rapid::ConnectionPtr &pConn, std::shared_ptr<WebSocketRequest> webSocketRequest) override {
		if (webSocketRequest->isClosed()) {
			auto pBuffer = pConn->getReceiveBuffer();
			pBuffer->retrieve(webSocketRequest->contentLength());
			onClose(pConn);
			return false;
		}
		pushMessage();
		return true;
	}

	virtual void onClose(rapid::ConnectionPtr &pConn) override {
		pusher_.unSubscribe(std::to_string(uid_));
		pTimer_->stop();
	}

private:
	void pushMessage() {
		using namespace rapidjson;

		Document doc(Type::kObjectType);
		Value host(Type::kObjectType);

		host.SetString("Server");
		doc.AddMember("host", host, doc.GetAllocator());

		Value time(rapidjson::Type::kObjectType);
		auto currentTime = std::to_string(std::time(nullptr));
		time.SetString(currentTime.c_str(), currentTime.length());
		doc.AddMember("time", time, doc.GetAllocator());

		Value counters(rapidjson::Type::kObjectType);

		for (auto const &counter : counters_) {
			Value counterValue(rapidjson::Type::kObjectType);
			auto current = std::to_string(counter->currentAsLong());
			counterValue.SetString(current.c_str(), current.length(), doc.GetAllocator());

			auto temp = rapid::utils::formUtf16(counter->path());
			Value counterPathName(rapidjson::Type::kStringType);
			counterPathName.SetString(temp.c_str(), temp.length(), doc.GetAllocator());
			counters.AddMember(counterPathName, counterValue, doc.GetAllocator());
		}

		doc.AddMember("counters", counters, doc.GetAllocator());

		StringBuffer buffer;
		PrettyWriter<StringBuffer> writer(buffer);
		doc.Accept(writer);

		pusher_.boardcast(std::string(buffer.GetString(), buffer.GetSize()));
	}

	uint32_t uid_;
	rapid::details::TimerPtr pTimer_;
	std::vector<std::unique_ptr<rapid::platform::PerformanceCounter>> counters_;
	MessagePusher pusher_;
};

WebSocketService::WebSocketService() {
}

std::shared_ptr<WebSocketService> WebSocketService::createService(std::string const & protocol) {
	if (protocol == "chat") {
		return std::make_shared<WebSocketChatService>();
	} else if (protocol == "perfmon") {
		return std::make_shared<WebSocketPerfmonService>();
	}
	return std::make_shared<WebSocketEchoService>();
}
