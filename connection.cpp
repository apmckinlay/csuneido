// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "connection.h"
#include "sockets.h"
#include <algorithm>
#include "except.h"
#include "fatal.h"
#include "exceptimp.h"
using std::min;
using std::max;

// NOTE: passes SocketConnect wrbuf to Serializer to access directly
Connection::Connection(SocketConnect* sc_)
	: Serializer(rdbuf, sc_->wrbuf), sc(*sc_) {
}

/// @post At least n bytes are available in rdbuf
void Connection::need(int n) {
	const int READSIZE = 1024;

	// NOTE: this is our rdbuf, not sc.rdbuf
	int toRead = n - rdbuf.remaining();
	if (toRead <= 0)
		return;
	int maxRead = max(toRead, READSIZE);
	char* buf = rdbuf.ensure(maxRead);
	int nr = sc.read(buf, toRead, maxRead);
	except_if(nr == 0, "lost connection");
	rdbuf.added(nr);
}

void Connection::read(char* dst, int n) {
	if (rdbuf.remaining()) {
		int take = min(n, rdbuf.remaining());
		memcpy(dst, rdbuf.getBuf(take), take);
		if (take == n)
			return; // got it all from rdbuf
		n -= take;
		dst += take;
	}
	int nr = sc.read(dst, n);
	except_if(nr == 0, "lost connection");
}

/// Write buffered data
void Connection::write() {
	write("", 0);
}

/// Write buffered data plus buf
void Connection::write(const char* buf, int n) {
	LIMIT(n);
	sc.write(buf, n);
	rdbuf.clear();
	// can't clear sc.wrbuf if using async because it doesn't block
}

void Connection::close() {
	sc.close();
}

//--------------------------------------------------------------------------------

#define DO(fn) \
	try { \
		fn; \
	} catch (const Except& e) { \
		fatal("lost connection:", e.str()); \
	}

void ClientConnection::need(int n) {
	DO(Connection::need(n));
}

void ClientConnection::write(const char* buf, int n) {
	DO(Connection::write(buf, n));
}

void ClientConnection::read(char* dst, int n) {
	DO(Connection::read(dst, n));
}
