//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <string>
#include <sstream>
#include <cstdio>

#include <rapid/exception.h>

#include <rapid/details/socketexception.h>
#include <rapid/details/contracts.h>
#include <rapid/details/socketaddress.h>

namespace rapid {

namespace details {

static addrinfo *getAddressInfo(char const *host, char const *service, AddressType atype) {
    // Create criteria for the socket we require.
	addrinfo addrCriteria = { 0 };
    addrCriteria.ai_family = AF_UNSPEC; // Any address family

    switch (atype) {
    case AddressType::TCP_SOCKET:
        // Ask for addresses for a TCP socket.
        addrCriteria.ai_socktype = SOCK_STREAM;
        addrCriteria.ai_protocol = IPPROTO_TCP;
        break;
    case AddressType::TCP_SERVER:
        // Ask for addresses for a TCP server socket
        addrCriteria.ai_socktype = SOCK_STREAM;
        addrCriteria.ai_protocol = IPPROTO_TCP;
        addrCriteria.ai_flags = AI_PASSIVE;
        break;
    case AddressType::UDP_SOCKET:
        // Ask for addresses for a UDP socket.
        addrCriteria.ai_socktype = SOCK_DGRAM;
        addrCriteria.ai_protocol = IPPROTO_UDP;
        break;
    }
    
	// Get linked list of remote addresses
    addrinfo *servAddrs;
	auto rtnVal = ::getaddrinfo(host, service, &addrCriteria, &servAddrs);
    if (rtnVal != 0) {
        throw Exception(gai_strerrorA(rtnVal));
    }
    return servAddrs;
}

bool isMulticast(SocketAddress const &sockaddr) noexcept {
    auto family = sockaddr.getFamily();

    if (family == AF_INET) {
        uint32_t addr = reinterpret_cast<sockaddr_in const *>(sockaddr.getSockAddr())->sin_addr.S_un.S_addr;
        return ((addr >= 0xE0000000) && (addr != 0xFFFFFFFF));
    } else if (family == AF_INET6) {
	    auto addr = reinterpret_cast<sockaddr_in6 const *>(sockaddr.getSockAddr())->sin6_addr.u.Byte[0];
        return addr == 0xFF;
    }
    return false;
}

bool SocketAddress::isIPv4Address(std::string const &address) {
    if (address.length() > 6 && address.length() < 16) {
        size_t numPeriods = 0;
        for (auto ch : address) {
            if (ch == '.') {
                numPeriods++;
            }
        }
        if (numPeriods == 3) {
            return true;
        }
    }
    return false;
}

bool SocketAddress::isIPv6Address(std::string const &address) {
    // Min address size: ::1
    // Max address size: FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
    if (address.length() > 2 && address.length() < 40) {
        uint32_t numColons = 0;
        for (auto ch : address) {
            if (ch == ':') {
                numColons++;
            }
        }
        if (numColons > 1 && numColons < 8) {
            return true;
        }
    }
    return false;
}

SocketAddress const SocketAddress::ANY("0.0.0.0", 0);

SocketAddress::SocketAddress() {
    clear();
}

SocketAddress::SocketAddress(std::string const &addr, uint16_t port) {
    clear();
    setFromString(addr, port);
}

SocketAddress::SocketAddress(struct sockaddr const *address, socklen_t len) {
    setFromSockAddr(address, len);
}

ADDRESS_FAMILY SocketAddress::getFamily() const {
    return getSockAddr()->sa_family;
}

bool SocketAddress::operator==(SocketAddress const &other) noexcept {
	if (getAddressLength() == other.getAddressLength()
		&& getFamily() == other.getFamily()) {
		return memcmp(&socketAddress_, other.getSockAddr(), getAddressLength()) == 0;
	}
	return false;
}

void SocketAddress::setFromSockAddr(struct sockaddr const *address, socklen_t len) {
    switch (address->sa_family) {
    case AF_INET:
        RAPID_ENSURE(sizeof(sockaddr_in) == len);
        memcpy(&socketAddress_, address, sizeof(sockaddr_in));
        addressLength_ = sizeof(sockaddr_in);
        break;
    case AF_INET6:
        RAPID_ENSURE(sizeof(sockaddr_in6) == len);
        memcpy(&socketAddress_, address, sizeof(sockaddr_in6));
        addressLength_ = sizeof(sockaddr_in6);
        break;
    default:
        RAPID_ENSURE(0 && "Unknown address type");
        break;
    }
}

void SocketAddress::setFromString(std::string const &addr, uint16_t port) {
    clear();
    int retval = 0;
    if (isIPv4Address(addr)) {
        retval = ::inet_pton(AF_INET, addr.c_str(), PVOID(&reinterpret_cast<sockaddr_in const *>(&socketAddress_)->sin_addr));
        if (retval > 0) {
            reinterpret_cast<sockaddr_in *>(&socketAddress_)->sin_family = AF_INET;
            reinterpret_cast<sockaddr_in *>(&socketAddress_)->sin_port = htons(port);
            addressLength_ = sizeof(sockaddr_in);
        }
    } else if (isIPv6Address(addr)) {
        retval = ::inet_pton(AF_INET6, addr.c_str(), PVOID(&reinterpret_cast<sockaddr_in6 const *>(&socketAddress_)->sin6_addr));
        if (retval > 0) {
            reinterpret_cast<sockaddr_in6 *>(&socketAddress_)->sin6_family = AF_INET6;
            reinterpret_cast<sockaddr_in6 *>(&socketAddress_)->sin6_port = htons(port);
            addressLength_ = sizeof(sockaddr_in6);
        }
    }
}

size_t SocketAddress::hash() const noexcept {
	auto sockAddr = getSockAddr();
	size_t hash = 0;
	switch (sockAddr->sa_family) {
	case AF_INET:
		hash ^= reinterpret_cast<sockaddr_in const*>(sockAddr)->sin_addr.S_un.S_addr;
		break;
	case AF_INET6:
		auto pIPV6address = 
			reinterpret_cast<uint32_t const*>(reinterpret_cast<sockaddr_in6 const*>(sockAddr));
		hash ^= pIPV6address[0] ^ pIPV6address[1] ^ pIPV6address[2] ^ pIPV6address[3];
		break;
	}
	hash ^= port() | (port() << 16);
	return hash;
}

in_port_t SocketAddress::port() const {
    auto sockAddr = getSockAddr();
    switch (sockAddr->sa_family) {
    case AF_INET:
        return ::ntohs(reinterpret_cast<sockaddr_in const *>(sockAddr)->sin_port);
        break;
    case AF_INET6:
        return ::ntohs(reinterpret_cast<sockaddr_in6 const *>(sockAddr)->sin6_port);
        break;
    default:
        RAPID_ENSURE(0 && "Unknown address type");
        break;
    }
}

struct sockaddr const * SocketAddress::getSockAddr() const noexcept {
    return reinterpret_cast<sockaddr const *>(&socketAddress_);
}

SocketAddress::operator const struct sockaddr * () const noexcept {
    return reinterpret_cast<const struct sockaddr *>(&socketAddress_);
}

SocketAddress::operator const struct sockaddr_in * () const noexcept {
    return reinterpret_cast<const struct sockaddr_in *>(&socketAddress_);
}

SocketAddress::operator const struct sockaddr_in6 * () const noexcept {
    return reinterpret_cast<const struct sockaddr_in6 *>(&socketAddress_);
}

SocketAddress::operator const struct sockaddr_storage * () const noexcept {
    return &socketAddress_;
}

socklen_t SocketAddress::getAddressLength() const noexcept {
    return addressLength_;
}

std::string SocketAddress::addressToString() const {
    char buffer[INET6_ADDRSTRLEN] = { 0 };
    auto sockAddr = getSockAddr();
    char const *str = nullptr;
    
	switch (getSockAddr()->sa_family) {
    case AF_INET:
        str = ::InetNtopA(AF_INET, &((sockaddr_in*)sockAddr)->sin_addr, buffer, INET_ADDRSTRLEN);
        break;
    case AF_INET6:
        str = ::InetNtopA(AF_INET6, &((sockaddr_in6*)sockAddr)->sin6_addr, buffer, INET6_ADDRSTRLEN);
        break;
    default:
        RAPID_ENSURE(0 && "Unknown address type");
        break;
    }

    if (!str) {
        throw Exception();
    }
    return buffer;
}

#pragma warning(push)
#pragma warning(disable : 4996)
std::string SocketAddress::toString() const {
    char buffer[INET6_ADDRSTRLEN] = { 0 };
    DWORD addrLen = INET6_ADDRSTRLEN;
    
	int retval;

    auto addr = getSockAddr();
    switch (addr->sa_family) {
    case AF_INET:
        retval = ::WSAAddressToStringA((sockaddr*)&socketAddress_, sizeof(sockaddr_in), nullptr, buffer, &addrLen);
        break;
    case AF_INET6:
        retval = ::WSAAddressToStringA((sockaddr*)&socketAddress_, sizeof(sockaddr_in6), nullptr, buffer, &addrLen);
        break;
    default:
        RAPID_ENSURE(0 && "Unknown address type");
        break;
    }

    if (retval < 0) {
        throw Exception(::GetLastError());
    }
    return buffer;
}
#pragma warning(pop)

void SocketAddress::clear() {
    memset(&socketAddress_, 0, sizeof(socketAddress_));
    addressLength_ = 0;
}

SocketAddress SocketAddress::getPeerName(SOCKET socketFd) {
    sockaddr_storage socketAddr;
    int addrLen = sizeof(socketAddr);

    if (::getpeername(socketFd, reinterpret_cast<sockaddr*>(&socketAddr), &addrLen) < 0) {
        throw Exception();
    }
    return SocketAddress(reinterpret_cast<sockaddr*>(&socketAddr), addrLen);
}

SocketAddress SocketAddress::getSockName(SOCKET socketFd) {
    sockaddr_storage socketAddr;
    int addrLen = sizeof(socketAddr);

    if (::getsockname(socketFd, reinterpret_cast<sockaddr*>(&socketAddr), &addrLen) < 0) {
        throw Exception();
    }
    return SocketAddress(reinterpret_cast<sockaddr*>(&socketAddr), addrLen);
}

std::unique_ptr<SOCKET_ADDRESS_LIST> toSocketAddressList(std::vector<SocketAddress> const &addressList) {
	auto allocBufferSize = SIZEOF_SOCKET_ADDRESS_LIST(addressList.size());

	auto buf = new uint8_t[allocBufferSize];
	auto pList = reinterpret_cast<SOCKET_ADDRESS_LIST *>(buf);
    std::unique_ptr<SOCKET_ADDRESS_LIST> list(pList);

    size_t i = 0;
    for (auto const &address : addressList) {
        pList[i].Address->lpSockaddr = const_cast<LPSOCKADDR>(address.getSockAddr());
        pList[i].Address->iSockaddrLength = sizeof(SOCKADDR_STORAGE);
        ++pList[i].iAddressCount;
    }
    return list;
}

std::vector<SocketAddress> lookupAddresses(std::string const &host, std::string const &service, AddressType atype) {
    std::vector<SocketAddress> addressList;
	auto serverAddress = getAddressInfo(host.c_str(), service.c_str(), atype);

    std::unique_ptr<addrinfo, void(*)(addrinfo *)> addrs(serverAddress, [](addrinfo *info) {
        ::freeaddrinfo(info);
    });

    for (auto pAddress = serverAddress; pAddress != nullptr; pAddress = pAddress->ai_next) {
        addressList.emplace_back(SocketAddress(pAddress->ai_addr, pAddress->ai_addrlen));
    }
    return addressList;;
}

std::vector<SocketAddress> lookupAddresses(std::string const &host, uint16_t port, AddressType atype) {
	auto service = std::to_string(port);

    std::vector<SocketAddress> addressList;
	auto servAddrs = getAddressInfo(host.c_str(), service.c_str(), atype);

    std::unique_ptr<addrinfo, void(*)(addrinfo *)> addrs(servAddrs, [](addrinfo *info) {
        ::freeaddrinfo(info);
    });

    for (auto curAddr = servAddrs; curAddr != nullptr; curAddr = curAddr->ai_next) {
        addressList.push_back(SocketAddress(static_cast<sockaddr *>(curAddr->ai_addr), curAddr->ai_addrlen));
    }
    return addressList;
}

}

}
