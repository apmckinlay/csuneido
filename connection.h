#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <cstdint>
#include "serializer.h"
#include "buffer.h"

class SocketConnect;
class gcstring;

class Value;

class Connection : public Serializer
{
public:
	explicit Connection(SocketConnect* sc_);

	void need(int n) override;
	void read(char * buf, int n) override;
	void write();
	virtual void write(const char * buf, int n);
	void close();

private:
	SocketConnect& sc;
	Buffer rdbuf; // can't use sc.rdbuf because of how sc uses it
};

/// A wrapper for a Connection that treats io exceptions as fatal
class ClientConnection : public Connection
	{
public:
	ClientConnection(SocketConnect* sc) : Connection(sc)
		{ }
	void need(int n) override;
	using Connection::write;
	void write(const char* buf, int n) override;
	void read(char* dst, int n) override;
	using Serializer::read;
	};

const int HELLO_SIZE = 50; // must match jSuneido
