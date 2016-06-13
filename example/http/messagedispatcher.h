//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <concurrent_unordered_map.h>
#include <memory>
#include <string>

#include <rapid/logging/logging.h>
#include <rapid/connection.h>

class MessageContext {
public:
    virtual ~MessageContext() = default;

    MessageContext(MessageContext const &) = delete;
    MessageContext& operator=(MessageContext const &) = delete;

	void serialize(rapid::IoBuffer *pBuffer) {
		doSerialize(pBuffer);
	}

	void unserialize(rapid::IoBuffer *pBuffer) {
		doUnserialize(pBuffer);
	}
protected:
    MessageContext() = default;

private:
	virtual void doSerialize(rapid::IoBuffer *pBuffer) {}
	virtual void doUnserialize(rapid::IoBuffer *pBuffer) {}
};

template <typename T>
class MessageDispatcher {
public:
    using EventHandler = std::function<void(rapid::ConnectionPtr&, std::shared_ptr<T>)>;

	MessageDispatcher() = default;

    MessageDispatcher(MessageDispatcher const &) = delete;
    MessageDispatcher& operator=(MessageDispatcher const &) = delete;

	void onMessage(std::string const &name, rapid::ConnectionPtr &pConn, std::shared_ptr<T> pMsg);

	void addMessageEventHandler(std::string const &name, EventHandler &&handler);
private:
	Concurrency::concurrent_unordered_map<std::string, EventHandler> handlers_;
};

template <typename T>
void MessageDispatcher<T>::onMessage(std::string const &name, rapid::ConnectionPtr &pConn, std::shared_ptr<T> pMsg) {
	auto itr = handlers_.find(name);
	if (itr != handlers_.end()) {
		(*itr).second(pConn, std::move(pMsg));
	}
}

template <typename T>
void MessageDispatcher<T>::addMessageEventHandler(std::string const &name, EventHandler &&handler) {
	handlers_[name] = std::forward<EventHandler>(handler);
}
