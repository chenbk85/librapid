//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <rapid/utils/singleton.h>
#include <rapid/utilis.h>

#include "http2frame.h"
#include "huffman.h"
#include "hpack.h"

#include "../httpconstants.h"
#include "../mime.h"

#include "http2pusher.h"

Http2Pusher::Http2Pusher()
	: promisedStreamID_(0) {	
}

Http2Pusher& Http2Pusher::add(std::string const &filePath) {
	resources_[filePath] = fileExtToMimeType(filePath);
	return *this;
}

Http2Pusher& Http2Pusher::add(std::string const& filePath, MimeEntry const& mime) {
	resources_[filePath] = mime;
	return *this;
}

void Http2Pusher::remove(std::string const& filePath) {
	resources_.erase(filePath);
}

void Http2Pusher::push(rapid::ConnectionPtr &pConn) {
	auto pBuffer = pConn->getSendBuffer();
	writePushPromiseFrame(pBuffer);
}

/*

+---------------+
|Pad Length? (8)|
+-+-------------+-----------------------------------------------+
|R|                  Promised Stream ID (31)                   |
+-+-----------------------------+-------------------------------+
|                   Header Block Fragment (*)                ...
+---------------------------------------------------------------+
|                           Padding (*)                      ...
+---------------------------------------------------------------+

*/
void Http2Pusher::writePushPromiseFrame(rapid::IoBuffer* pBuffer) {
	auto pFrameStart = pBuffer->writeData();

	Http2Frame frame;
	frame.type = H2_FRAME_PUSH_PROMISE;
	pBuffer->advanceWriteIndex(H2_FRAME_SIZE);

	writeHttp2Frame(pBuffer, frame, pFrameStart);
	uint8_t pushPromiseFramePaddingLen = 0;
	rapid::writeData(pBuffer, pushPromiseFramePaddingLen);
	rapid::writeData(pBuffer, promisedStreamID_);

	for (auto const &resource : resources_) {
		buildPushRequestHeader(pBuffer, resource.first);
	}
}

void Http2Pusher::buildPushRequestHeader(rapid::IoBuffer *pBuffer, std::string const& resource) {
	Http2Hpack::encodeHeader(pBuffer, H2_HEADER_METHOD, HTTP_METHOD_GET);
	Http2Hpack::encodeHeader(pBuffer, H2_HEADER_PATH, HTTP_HOST.str());
	Http2Hpack::encodeHeader(pBuffer, H2_HEADER_AUTHORITY, HTTP_HOST.str());

}

void Http2Pusher::setPromisedStreamID(uint32_t streamID) {
	promisedStreamID_ = streamID;
}

void Http2Pusher::onPush(rapid::ConnectionPtr &pConn) {
}
