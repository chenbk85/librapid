#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <rapid/iobuffer.h>
#include <rapid/platform/performancecounter.h>
#include <rapid/platform/utils.h>
#include <rapid/logging/logging.h>
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

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn) override {
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

class WebSocketChatService : public WebSocketService {
public:
	WebSocketChatService() {

	}

	virtual ~WebSocketChatService() {

	}

	virtual void onWebSocketOpen(rapid::ConnectionPtr &pConn) override {
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

WebSocketService::WebSocketService() {
}

std::shared_ptr<WebSocketService> WebSocketService::createService(std::string const & protocol) {
	if (protocol == "chat") {
		return createChatService();
	} else if (protocol == "perfmon") {
		return createPerfmonService();
	}
	return createChatService();
}

std::shared_ptr<WebSocketService> WebSocketService::createPerfmonService() {
	return std::make_shared<WebSocketPerfmonService>();
}

std::shared_ptr<WebSocketService> WebSocketService::createChatService() {
	return std::make_shared<WebSocketChatService>();
}

