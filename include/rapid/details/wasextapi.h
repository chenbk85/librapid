//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <Winsock2.h>
#include <mswsock.h>
#include <Mstcpip.h>

#include <rapid/utils/singleton.h>
#include <rapid/details/socket.h>
#include <rapid/details/socketaddress.h>

namespace rapid {

namespace details {

class WsaExtAPI : public utils::Singleton<WsaExtAPI> {
public:
    WsaExtAPI(WsaExtAPI const &) = delete;
    WsaExtAPI& operator=(WsaExtAPI const &) = delete;

    bool connectEx(TcpSocket const &socket,
                   SocketAddress const &address,
                   PVOID sendBuffer,
                   uint32_t sendDataLength,
                   LPDWORD byteSent,
                   LPOVERLAPPED overlapped) const noexcept;

    bool acceptEx(TcpServerSocket const &listenSocket,
                  TcpSocket const &acceptSocket,
                  PVOID output,
                  uint32_t recvLen,
                  uint32_t localAddrLen,
                  uint32_t remoteAddrLen,
                  LPDWORD bytesRead,
                  OVERLAPPED *overlapped) const noexcept;

    void getAcceptAddress(PVOID output,
                          uint32_t recvLen,
                          uint32_t localAddrLen,
                          uint32_t remoteAddrLen,
                          LPSOCKADDR *localAddr,
                          LPINT localSockAddrLen,
                          LPSOCKADDR *remoteAddr,
                          LPINT remoteSockAddrLen) const noexcept;

    bool disconnectEx(TcpSocket const &socket,
                      OVERLAPPED *overlapped,
                      uint32_t flags,
                      uint32_t reserved) const noexcept;

    bool transmitFile(TcpSocket const &socket,
                      HANDLE fileHandle,
                      uint32_t numberOfBytesToWrite,
                      uint32_t numberOfBytesPerSend,
                      LPOVERLAPPED overlapped,
                      LPTRANSMIT_FILE_BUFFERS transmitFileBuffer,
                      uint32_t reserved) const noexcept;

    void setSkipIoSyncNotify(Socket const &socket) const;

    bool hasIFSHandleInstalled() const noexcept;

	static void cancelPendingIoRequest(Socket const &socket, OVERLAPPED *overlapped);

    void cancelAllPendingIoRequest(Socket const &socket) const;

private:
    friend class utils::Singleton<WsaExtAPI>;
    WsaExtAPI();

    LPFN_DISCONNECTEX pfnDisconnectEx_;
    LPFN_ACCEPTEX pfnAcceptEx_;
    LPFN_GETACCEPTEXSOCKADDRS pfnGetAcceptExAddrs_;
    LPFN_CONNECTEX pfnConnectEx_;
    LPFN_TRANSMITFILE pfnTransmitFile_;
	
    bool isIfsHandleFlag_;
};

__forceinline bool WsaExtAPI::connectEx(TcpSocket const &socket,
                                    SocketAddress const &address,
                                    PVOID sendBuffer,
                                    uint32_t sendDataLength,
                                    LPDWORD byteSent,
                                    LPOVERLAPPED overlapped) const throw() {
    return (*pfnConnectEx_)(socket.socketFd(),
                           address.getSockAddr(),
                           address.getAddressLength(),
                           sendBuffer,
                           sendDataLength,
                           byteSent,
                           overlapped) != FALSE;
}

__forceinline bool WsaExtAPI::acceptEx(TcpServerSocket const &listenSocket,
                                   TcpSocket const &acceptSocket,
                                   PVOID output,
                                   uint32_t recvLen,
                                   uint32_t localAddrLen,
                                   uint32_t remoteAddrLen,
                                   LPDWORD bytesRead,
                                   OVERLAPPED *overlapped) const noexcept {
    return (*pfnAcceptEx_)(listenSocket.socketFd(),
                          acceptSocket.socketFd(),
                          output,
                          recvLen,
                          localAddrLen,
                          remoteAddrLen,
                          bytesRead,
                          overlapped) != FALSE;
}

__forceinline void WsaExtAPI::getAcceptAddress(PVOID output,
        uint32_t recvLen,
        uint32_t localAddrLen,
        uint32_t remoteAddrLen,
        LPSOCKADDR *localAddr,
        LPINT localSockAddrLen,
        LPSOCKADDR *remoteAddr,
        LPINT remoteSockAddrLen) const noexcept {
    (*pfnGetAcceptExAddrs_)(output,
                           recvLen,
                           localAddrLen,
                           remoteAddrLen,
                           localAddr,
                           localSockAddrLen,
                           remoteAddr,
                           remoteSockAddrLen);
}

__forceinline bool WsaExtAPI::disconnectEx(TcpSocket const &socket,
                                       OVERLAPPED *overlapped,
                                       uint32_t flags,
                                       uint32_t reserved) const noexcept {
    return (*pfnDisconnectEx_)(socket.socketFd(),overlapped, flags, reserved) != FALSE;
}

__forceinline bool WsaExtAPI::transmitFile(TcpSocket const &socket,
                                       HANDLE fileHandle,
                                       uint32_t numberOfBytesToWrite,
                                       uint32_t numberOfBytesPerSend,
                                       LPOVERLAPPED overlapped,
                                       LPTRANSMIT_FILE_BUFFERS transmitFileBuffer,
                                       uint32_t reserved) const noexcept {
    return (*pfnTransmitFile_)(socket.socketFd(),
                              fileHandle,
                              numberOfBytesToWrite,
                              numberOfBytesPerSend,
                              overlapped,
                              transmitFileBuffer,
                              reserved) != FALSE;
}

}

}
