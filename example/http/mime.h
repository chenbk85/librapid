//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

struct MimeEntry {
	std::string toString() const;
	std::string type;
	std::string contentType;
};

MimeEntry fileExtToMimeType(std::string const &fileExt);

bool isCompressibleable(MimeEntry const &mimeEntry);
