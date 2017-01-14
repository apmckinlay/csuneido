#ifndef SOCKETS_H
#define SOCKETS_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2000 Suneido Software Corp. 
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

#include "buffer.h"

// abstract base class (interface) for socket connections
class SocketConnect
	{
public:
	virtual ~SocketConnect()
		{ }

	/// Takes/removes data from rdbuf (leftover from readline) 
	///	and then directly from socket.
	/// Only reads what's required.
	/// Subclasses normally implement either read(buf,n) or read(buf, reqd, max)
	virtual int read(char* buf, int n)
		{
		return read(buf, n, n); // bufsize = n, no extra read
		}

	/// Same as read(buf, n) but reads extra data, up to max.
	/// This can avoid too many socket read calls, e.g. to read one byte at a time.
	/// Subclasses normally implement either read(buf, n) or read(buf, reqd, max)
	virtual int read(char* buf, int required, int bufsize)
		{
		return read(buf, required); // bufsize ignored
		}

	/// Read up to the next newline using read()
	/// May leave extra data in rdbuf.
	virtual bool readline(char* buf, int n) = 0;

	/// Gathering write of wrbuf and buf argument
	/// wrbuf is left empty.
	virtual void write(char* buf, int n) = 0;

	/// Add to wrbuf
	void writebuf(char* buf, int n)
		{ wrbuf.add(buf, n); }

	void write(char* s);
	void writebuf(char* s);
	virtual void close() = 0;
	virtual void* getarg()
		{ return nullptr; }

	// used by dbserver and susockets Socket.RemoteUser
	// only implemented in SocketConnectAsynch
	virtual char* getadr()
		{ return ""; }

	Buffer rdbuf;
	Buffer wrbuf;
	};

// start a socket server (to listen)
// calls supplied newserver function for connections
typedef void (_stdcall *NewServerConnection)(void*);
void socketServer(char* title, int port, NewServerConnection newServerConn, void* arg, bool exit);

// create a synchronous (blocks everything) socket connection
SocketConnect* socketClientSync(char* addr, int port, int timeout = 9999, int timeoutConnect = 0);

// create an asynch (only blocks calling fiber) socket connection
SocketConnect* socketClientAsync(char* addr, int port, int timeout = 9999, int timeoutConnect = 10);

#endif
