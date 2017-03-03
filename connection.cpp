/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation - version 2.
*
* This program is distributed in the hope that it will be
* useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE.  See the GNU General Public License in the file COPYING
* for more details.
*
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the Free
* Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "connection.h"
#include "sockets.h"
#include <algorithm>
#include "except.h"
#include "fatal.h"
#include "exceptimp.h"
using std::min;
using std::max;

// NOTE: passes SocketConnect wrbuf to Serializer to access directly
Connection::Connection(SocketConnect* sc_) : Serializer(rdbuf, sc_->wrbuf), sc(*sc_)
	{ }

/// @post At least n bytes are available in rdbuf
void Connection::need(int n)
	{
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

void Connection::read(char* dst, int n)
	{
	if (rdbuf.remaining())
		{
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
void Connection::write()
	{
	write("", 0);
	}

/// Write buffered data plus buf
void Connection::write(const char* buf, int n)
	{
	LIMIT(n);
	sc.write(buf, n);
	rdbuf.clear();
	// can't clear sc.wrbuf if using async because it doesn't block
	}

void Connection::close()
	{
	sc.close();
	}

//--------------------------------------------------------------------------------

#define DO(fn) try { fn; } \
	catch (const Except& e) { fatal("lost connection:", e.str()); }

void ClientConnection::need(int n)
	{
	DO(Connection::need(n));
	}

void ClientConnection::write(const char* buf, int n)
	{
	DO(Connection::write(buf, n));
	}

void ClientConnection::read(char* dst, int n)
	{
	DO(Connection::read(dst, n));
	}

