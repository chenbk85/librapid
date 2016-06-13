//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <rapid/connection.h>

#include "../mime.h"

class Http2Pusher {
public:
	Http2Pusher();

	Http2Pusher& add(std::string const &filePath);

	Http2Pusher& add(std::string const &filePath, MimeEntry const &mime);

	void remove(std::string const &filePath);

	void setPromisedStreamID(uint32_t streamID);

	void push(rapid::ConnectionPtr &pConn);

private:
	void writePushPromiseFrame(rapid::IoBuffer *pBuffer);

	void buildPushRequestHeader(rapid::IoBuffer *pBuffer, std::string const &resource);

	void onPush(rapid::ConnectionPtr &pConn);

	std::unordered_map<std::string, MimeEntry> resources_;
	uint32_t promisedStreamID_;
};
