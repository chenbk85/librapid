//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma comment(lib, "Ws2_32.lib")

#include <rapid/platform/platform.h>
#include <mstcpip.h>

#include <rapid/details/socketexception.h>
#include <rapid/details/socket.h>

namespace rapid {

namespace details {

Socket::Socket(SOCKET sd)
    : sockDesc_(sd) {
}

Socket::Socket(int family, int type, int protocol) {
    sockDesc_ = ::socket(family, type, protocol);
    if (sockDesc_  == SOCKET_ERROR) {
        throw SocketException("Socket creation failed (socket())");
    }
}

Socket::~Socket() {
    if (sockDesc_ < 0) {
        return;
    }
	::closesocket(sockDesc_);
}

void Socket::shutdown(int how) const {
	::shutdown(sockDesc_, how);
}

void Socket::shutdownSend() const {
	shutdown(SD_SEND);
}

void Socket::shutdownRecv() const {
	shutdown(SD_RECEIVE);
}

bool Socket::ioctl(uint32_t flags) const {
    static uint32_t enable = 1;
    DWORD bytes = 0;
    auto retval = ::WSAIoctl(sockDesc_,
                             flags,
                             &enable,
                             sizeof(enable),
                             nullptr,
                             0,
                             &bytes,
                             nullptr,
                             nullptr);
    return (retval != SOCKET_ERROR);
}

SocketAddress Socket::getLocalAddress() const {
    sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    if (::getsockname(sockDesc_, reinterpret_cast<sockaddr *>(&addr),
                      static_cast<socklen_t *>(&addrLen)) < 0) {
        throw SocketException("Fetch of local address failed (getsockname())");
    }
    return SocketAddress(reinterpret_cast<sockaddr *>(&addr), addrLen);
}

void Socket::setLocalPort(unsigned short localPort) const {
    sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(localPort);
    if (::bind(sockDesc_, reinterpret_cast<sockaddr *>(&localAddr), sizeof(sockaddr_in)) < 0) {
        throw SocketException("Set of local port failed (bind())");
    }
}

void Socket::setLocalAddressAndPort(std::string const &localAddress, unsigned short localPort) const {
    SocketAddress localAddr(localAddress, localPort);
    if (::bind(sockDesc_, localAddr.getSockAddr(), localAddr.getAddressLength()) < 0) {
        throw SocketException("Set of local address and port failed (bind())");
    }
}

CommunicatingSocket::CommunicatingSocket(int family, int type, int protocol)
    : Socket(family, type, protocol) {
}

CommunicatingSocket::CommunicatingSocket(SOCKET newConnSD)
    : Socket(newConnSD) {
}

void CommunicatingSocket::send(const void *buffer, int bufferLen) const {
    if (::send(sockDesc_, (raw_type *)buffer, bufferLen, 0) < 0) {
        throw SocketException("Send failed (send())");
    }
}

int CommunicatingSocket::recv(void *buffer, int bufferLen) const {
    auto bytesRead = ::recv(sockDesc_, (raw_type *)buffer, bufferLen, 0);
    if (bytesRead < 0) {
        throw SocketException("Received failed (recv())");
    }
    return bytesRead;
}

SocketAddress CommunicatingSocket::getForeignAddress() const {
    sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    if (::getpeername(sockDesc_, reinterpret_cast<sockaddr *>(&addr), static_cast<socklen_t *>(&addrLen)) < 0) {
        throw SocketException("Fetch of foreign address failed (getpeername())");
    }
    return SocketAddress(reinterpret_cast<sockaddr *>(&addr), addrLen);
}

void CommunicatingSocket::connect(std::string const &foreignAddress, unsigned short foreignPort) const {
    SocketAddress destAddr(foreignAddress, foreignPort);
    if (::connect(sockDesc_, destAddr.getSockAddr(), destAddr.getAddressLength()) < 0) {
        throw SocketException("Connect failed (connect())");
    }
}

TcpSocket::TcpSocket(int family)
    : CommunicatingSocket(family, SOCK_STREAM, IPPROTO_TCP) {
}

TcpSocket::TcpSocket(std::string const &foreignAddress, unsigned short foreignPort)
    : CommunicatingSocket(SocketAddress::isIPv4Address(foreignAddress) ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP) {
    connect(foreignAddress, foreignPort);
}

TcpSocket::TcpSocket(SOCKET newConnSD)
    : CommunicatingSocket(newConnSD) {
}

TcpServerSocket::TcpServerSocket(unsigned short localPort, int queueLen, int family)
    : Socket(family, SOCK_STREAM, IPPROTO_TCP) {
    setLocalPort(localPort);
    setListen(queueLen);
}

TcpServerSocket::TcpServerSocket(std::string const &localAddress,
                                 unsigned short localPort,
                                 int queueLen)
    : Socket(SocketAddress::isIPv4Address(localAddress) ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP) {

    if (IsWindows8OrGreater()) {
        if (!ioctl(SIO_LOOPBACK_FAST_PATH)) {
            throw SocketException("Set SIO_LOOPBACK_FAST_PATH failed (WSAIoctl())");
        }
    }

    // 不需要等待一個前一個連接的TIME-WAIT狀態就可以重新啟動Server.
	auto const enable = 1;
    if (setSockOpt(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &enable) < 0) {
        throw SocketException("Set SO_REUSEADDR failed (setsockopt())");
    }

	int sendBufferSize = 0;
	if (setSockOpt(SOL_SOCKET, SO_SNDBUF, &sendBufferSize) < 0) {
		throw SocketException("Set send buffer size failed (setsockopt())");
	}

    setLocalAddressAndPort(localAddress, localPort);
    setListen(queueLen);
}

std::unique_ptr<TcpSocket> TcpServerSocket::accept() const {
    auto newConnSD = ::accept(sockDesc_, nullptr, nullptr);
    if (newConnSD == SOCKET_ERROR) {
        throw SocketException("Accept failed (accept())");
    }
    return std::unique_ptr<TcpSocket>(new TcpSocket(newConnSD));
}

void TcpServerSocket::setListen(int queueLen) const {
    if (::listen(sockDesc_, queueLen) < 0) {
        throw SocketException("Set listening socket failed (listen())");
    }
}

}

}