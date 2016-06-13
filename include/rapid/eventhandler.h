//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <functional>

namespace rapid {

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using WeakTcpClientChannelPtr = std::weak_ptr<Connection>;

using ContextEventHandler = std::function<void(ConnectionPtr&)>;

}