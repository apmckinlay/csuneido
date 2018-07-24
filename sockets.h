#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "buffer.h"
#include "gcstring.h"

// abstract base class (interface) for socket connections
class SocketConnect {
public:
	virtual ~SocketConnect() = default;

	/// Takes/removes data from rdbuf (leftover from readline)
	///	and then directly from socket.
	/// Only reads what's required.
	/// Subclasses normally implement either read(buf,n) or read(buf, reqd, max)
	virtual int read(char* buf, int n) {
		return read(buf, n, n); // bufsize = n, no extra read
	}

	/// Same as read(buf, n) but reads extra data, up to max.
	/// This can avoid too many socket read calls e.g. to read 1 byte at a time
	/// Subclasses normally implement read(buf, n) or read(buf, reqd, max)
	virtual int read(char* buf, int required, int bufsize) {
		return read(buf, required); // bufsize ignored
	}

	gcstring read(int n) {
		char* buf = salloc(n);
		read(buf, n);
		return gcstring::noalloc(buf, n);
	}

	/// Read up to the next newline using read()
	/// May leave extra data in rdbuf.
	virtual bool readline(char* buf, int n) = 0;

	/// Gathering write of wrbuf and buf argument
	/// wrbuf is left empty.
	virtual void write(const char* buf, int n) = 0;

	void write(const char* s);
	void write(const gcstring& s);

	/// Add to wrbuf
	void writebuf(const char* buf, int n) {
		wrbuf.add(buf, n);
	}

	void writebuf(const char* s);
	void writebuf(const gcstring& s);

	virtual void close() = 0;
	virtual void* getarg() {
		return nullptr;
	}

	// used by dbserver and susockets Socket.RemoteUser
	// only implemented in SocketConnectAsynch
	virtual const char* getadr() {
		return "";
	}

	Buffer rdbuf;
	Buffer wrbuf;
};

// start a socket server (to listen)
// calls supplied newserver function for connections
typedef void(_stdcall* NewServerConnection)(void*);
void socketServer(const char* title, int port,
	NewServerConnection newServerConn, void* arg, bool exit);

// create a synchronous (blocks everything) socket connection
SocketConnect* socketClientSync(
	const char* addr, int port, int timeout = 9999, int timeoutConnect = 0);

// create an asynch (only blocks calling fiber) socket connection
SocketConnect* socketClientAsync(
	const char* addr, int port, int timeout = 9999, int timeoutConnect = 10);
