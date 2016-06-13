//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <memory>

#include <rapid/platform/platform.h>
#include <rapid/details/socketaddress.h>

namespace rapid {

namespace details {

template <typename T>
inline bool getSocketExFunPtr(SOCKET socket,
        T *address,
        GUID const *guid,
        DWORD controlCode) {
        DWORD bytes = 0;
    return ::WSAIoctl(socket,
        controlCode,
        LPVOID(guid),
        sizeof(GUID),
        address,
        sizeof(T),
        &bytes,
        nullptr,
        nullptr) != SOCKET_ERROR;
}

class Socket {
public:
    Socket(Socket const &) = delete;
    Socket& operator=(Socket const &) = delete;

    virtual ~Socket();

    void shutdownSend() const;

    void shutdownRecv() const;

    SocketAddress getLocalAddress() const;

    void setLocalPort(unsigned short localPort) const;

    void setLocalAddressAndPort(std::string const &localAddress, unsigned short localPort) const;

    template <typename T>
    int getSockOpt(int level, int optname, T* optval, socklen_t* optlen) const noexcept {
        return ::getsockopt(sockDesc_, level, optname, reinterpret_cast<char*>(optval), optlen);
    }

    template <typename T>
    int setSockOpt(int level, int optname, T const *optval) const noexcept {
        return ::setsockopt(sockDesc_, level, optname, reinterpret_cast<char const *>(optval), sizeof(T));
    }

    bool ioctl(uint32_t flags) const;

	SOCKET socketFd() const noexcept;

	HANDLE handle() const noexcept;

protected:
    void shutdown(int how) const;

    explicit Socket(SOCKET sd);
    Socket(int family, int type, int protocol);
    SOCKET sockDesc_;
};

__forceinline SOCKET Socket::socketFd() const noexcept {
	return sockDesc_;
}

__forceinline HANDLE Socket::handle() const noexcept {
	return reinterpret_cast<HANDLE>(sockDesc_);
}

class CommunicatingSocket : public Socket {
public:
    virtual ~CommunicatingSocket() = default;

    void connect(std::string const &foreignAddress, unsigned short foreignPort) const;

    void send(void const *buffer, int bufferLen) const;

    int recv(void *buffer, int bufferLen) const;

    SocketAddress getForeignAddress() const;

protected:
    CommunicatingSocket(int family, int type, int protocol);
    explicit CommunicatingSocket(SOCKET newConnSD);
};

class TcpSocket : public CommunicatingSocket {
public:
    explicit TcpSocket(int family = AF_INET);

    TcpSocket(std::string const &foreignAddress, unsigned short foreignPort);

    virtual ~TcpSocket() = default;

private:
    friend class TcpServerSocket;
    explicit TcpSocket(SOCKET newConnSD);
};

class TcpServerSocket : public Socket {
public:
    explicit TcpServerSocket(unsigned short localPort, int queueLen = SOMAXCONN, int family = AF_INET);

    virtual ~TcpServerSocket() = default;

    TcpServerSocket(std::string const &localAddress, unsigned short localPort, int queueLen);

    std::unique_ptr<TcpSocket> accept() const;

private:
    void setListen(int queueLen) const;
};

}

}