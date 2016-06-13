#pragma once

#include <rapid/connection.h>

class TransportCodec {
public:
	virtual ~TransportCodec() = default;

	TransportCodec(TransportCodec const &) = delete;
	TransportCodec& operator=(TransportCodec const &) = delete;

	virtual void readLoop(rapid::ConnectionPtr &pConn, size_t &bytesToRead) = 0;

protected:
	TransportCodec() = default;
};
