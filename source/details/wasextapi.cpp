//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <vector>

#include <rapid/exception.h>
#include <rapid/platform/utils.h>

#include <rapid/details/contracts.h>
#include <rapid/logging/logging.h>
#include <rapid/details/socket.h>

#include <rapid/details/wasextapi.h>

namespace rapid {

namespace details {

static bool checkIFSProviders() {
    TcpSocket tmp;
    
	WSAPROTOCOL_INFOW protocolInfo = { 0 };

    socklen_t len = sizeof(protocolInfo);
    
	auto retval = tmp.getSockOpt(SOL_SOCKET, SO_PROTOCOL_INFOW, &protocolInfo, &len);
    if (retval < 0) {
        throw Exception();
    }
    
	// If the Service Flags value has the 0x20000 bit set,
	// the provider uses IFS handles and will work correctly.
	// If the 0x20000 bit is clear (not set), it is a non-IFS BSP or LSP.
	if ((protocolInfo.dwServiceFlags1 & XP1_IFS_HANDLES) == 0) {
        return false;
    }
    return true;
}

WsaExtAPI::WsaExtAPI()
	: isIfsHandleFlag_(false) {

	isIfsHandleFlag_ = checkIFSProviders();

	RAPID_LOG_IF(rapid::logging::Warn, !isIfsHandleFlag_) 
		<< "isIfsHandleFlag=" << (isIfsHandleFlag_ ? "True" : "False");

    TcpSocket tmp;

	auto constexpr controlCode = SIO_GET_EXTENSION_FUNCTION_POINTER;

	static GUID constexpr ACCEPTEX_GUID = WSAID_ACCEPTEX;
	static GUID constexpr GETACCEPTSOCKADDRS_GUID = WSAID_GETACCEPTEXSOCKADDRS;
	static GUID constexpr DISCONNECTEX_GUID = WSAID_DISCONNECTEX;
	static GUID constexpr CONNECTEX_GUID = WSAID_CONNECTEX;
	static GUID constexpr TRANSMITFILE_GUID = WSAID_TRANSMITFILE;

	if (getSocketExFunPtr(tmp.socketFd(), &pfnDisconnectEx_, &DISCONNECTEX_GUID, controlCode)
		&& getSocketExFunPtr(tmp.socketFd(), &pfnAcceptEx_, &ACCEPTEX_GUID, controlCode)
		&& getSocketExFunPtr(tmp.socketFd(), &pfnGetAcceptExAddrs_, &GETACCEPTSOCKADDRS_GUID, controlCode)
		&& getSocketExFunPtr(tmp.socketFd(), &pfnConnectEx_, &CONNECTEX_GUID, controlCode)
		&& getSocketExFunPtr(tmp.socketFd(), &pfnTransmitFile_, &TRANSMITFILE_GUID, controlCode)) {
    } else {
        throw Exception();
    }
}

bool WsaExtAPI::hasIFSHandleInstalled() const noexcept {
	return isIfsHandleFlag_;
}

void WsaExtAPI::setSkipIoSyncNotify(Socket const &socket) const {
    UCHAR mode = FILE_SKIP_SET_EVENT_ON_HANDLE;

	if (isIfsHandleFlag_) {
        mode |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    }

    if (!::SetFileCompletionNotificationModes(reinterpret_cast<HANDLE>(socket.socketFd()), mode)) {
        throw Exception();
    }
}

void WsaExtAPI::cancelPendingIoRequest(Socket const &socket, OVERLAPPED *overlapped) {
    auto const retval = ::CancelIoEx(reinterpret_cast<HANDLE>(socket.socketFd()), overlapped);
    
	auto lastError = ::GetLastError();

    if (retval == TRUE && lastError != ERROR_NOT_FOUND) {
        DWORD numTransferBytes = 0;
        if (!::GetOverlappedResult(reinterpret_cast<HANDLE>(socket.socketFd()), overlapped, &numTransferBytes, TRUE)) {
			RAPID_LOG_WARN() << __FUNCTION__ << " return failure! error=" << lastError;
        }
    }
}

void WsaExtAPI::cancelAllPendingIoRequest(Socket const &socket) const {
	if (!isIfsHandleFlag_) {
        DWORD bytes = 0;
        auto socketFd = socket.socketFd();
        // If there are non-ifs LSPs then try to obtain a base handle for the
        // socket.
        if (::WSAIoctl(socketFd,
                       SIO_BASE_HANDLE,
                       nullptr,
                       0,
                       &socketFd,
                       sizeof(socketFd),
                       &bytes,
                       nullptr,
                       nullptr) != 0) {
            // Failed. We can't do CancelIo.
            return;
        }
    }

    OVERLAPPED overlapped = { 0 };
    auto retval = ::CancelIo(reinterpret_cast<HANDLE>(socket.socketFd()));
    auto lastError = ::GetLastError();

    if (retval == TRUE && lastError != ERROR_NOT_FOUND) {
        DWORD bytesTransferred = 0;
        if (!::GetOverlappedResult(reinterpret_cast<HANDLE>(socket.socketFd()), &overlapped, &bytesTransferred, TRUE)) {
            RAPID_LOG_WARN() << __FUNCTION__ << " return failure! error=" << lastError;
        }
    }
}

}

}
