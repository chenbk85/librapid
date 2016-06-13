//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <unordered_set>
#include <unordered_map>

#include <rapid/logging/logging.h>

#include "mime.h"

static MimeEntry const MIME_APP_OCTET_STREAM{
	"application", "application/octet-stream"
};

// HTTP MIME map
static std::unordered_map<std::string, MimeEntry> s_mimeTypeMap {
	{ ".html", { "text", "text/html" } },
	{ ".css", { "text", "text/css" } },
	{ ".jpg", { "image", "image/jpeg" } },
	{ ".jpeg", { "image", "image/jpeg" } },
	{ ".js", { "text", "application/javascript" } },
	{ ".ico", { "image", "image/x-icon" } },
	{ ".gif", { "image", "image/gif" } },
	{ ".png", { "image", "image/png" } },
	{ ".mp4", { "video", "video/mp4" } },
	{ ".ogg", { "video", "video/ogg" } },
	{ ".webm", { "video", "video/webm" } },
	{ ".woff", { "application", "application/font-woff" } },
};

static std::unordered_set<std::string> const s_compressMimeType{
	"application/javascript",
	"application/json",
	"application/x-javascript",
	"application/xhtml+xml",
	"application/xml+rss",
	"text/css",
	"text/html",
	"text/javascript",
	"text/plain",
	"text/xml"
};

std::string MimeEntry::toString() const {
	return contentType;
}

MimeEntry fileExtToMimeType(std::string const &fileExt) {
	auto itr = s_mimeTypeMap.find(fileExt);
	if (itr != s_mimeTypeMap.end()) {
		return (*itr).second;
	}
	RAPID_LOG_INFO() << "Not found mime type: " << fileExt;
	return MIME_APP_OCTET_STREAM;
}

bool isCompressibleable(MimeEntry const &mimeEntry) {
	return s_compressMimeType.find(mimeEntry.toString()) != s_compressMimeType.end();
}
