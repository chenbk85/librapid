//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <memory>

#include <ws2tcpip.h>
#include <Winsock2.h>

typedef unsigned short in_port_t;

namespace rapid {

namespace details {

enum AddressType {
    TCP_SOCKET,
    TCP_SERVER,
    UDP_SOCKET,
};

class SocketAddress final {
public:
    static SocketAddress const ANY;

    static bool isIPv4Address(std::string const &address);

    static bool isIPv6Address(std::string const &address);

    static SocketAddress getPeerName(SOCKET socketFd);

    static SocketAddress getSockName(SOCKET socketFd);

    SocketAddress();

    SocketAddress(SocketAddress const &other);

    SocketAddress& operator=(SocketAddress const &other);

	bool operator==(SocketAddress const &other) noexcept;

    SocketAddress(struct sockaddr const *address, socklen_t len);

    SocketAddress(std::string const &addr, uint16_t port);

    void setFromSockAddr(struct sockaddr const *address, socklen_t len);

    void setFromString(std::string const &addr, uint16_t port);

	size_t hash() const noexcept;

    void clear();

    std::string toString() const;

    std::string addressToString() const;

    in_port_t port() const;

    ADDRESS_FAMILY getFamily() const;

    struct sockaddr const * getSockAddr() const noexcept;

    socklen_t getAddressLength() const noexcept;

    operator const struct sockaddr * () const noexcept;

    operator const struct sockaddr_in * () const noexcept;

    operator const struct sockaddr_in6 * () const noexcept;

    operator const struct sockaddr_storage * () const noexcept;

private:
    struct sockaddr_storage socketAddress_;
    socklen_t addressLength_;
};

inline SocketAddress::SocketAddress(SocketAddress const &other) {
    *this = other;
}

inline SocketAddress& SocketAddress::operator=(SocketAddress const &other) {
	if (this != &other) {
		memcpy(&socketAddress_, &other.socketAddress_, sizeof(other.socketAddress_));
		addressLength_ = other.addressLength_;
	}
    return *this;
}

std::unique_ptr<SOCKET_ADDRESS_LIST> toSocketAddressList(std::vector<SocketAddress> const &addrList);

std::vector<SocketAddress> lookupAddresses(std::string const &host, std::string const &service, AddressType atype);

std::vector<SocketAddress> lookupAddresses(std::string const &host, uint16_t port, AddressType atype);

bool isMulticast(SocketAddress const &sockaddr) noexcept;

struct SocketAddressPair {
    SocketAddressPair() {
    }
    SocketAddressPair(SocketAddress const &local, SocketAddress const &remote)
        : localAddress(local)
        , remoteAddess(remote) {
    }
    SocketAddress localAddress;
    SocketAddress remoteAddess;
};

}

}
