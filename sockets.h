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
	virtual int read(char* buf, int n) = 0;
	virtual bool readline(char* buf, int n) = 0;
	virtual void write(char* buf, int n) = 0;
	void writebuf(char* buf, int n)
		{ wrbuf.add(buf, n); }
	void write(char* s);
	void writebuf(char* s);
	virtual void close() = 0;
	virtual void* getarg()
		{ return 0; }

	// used by dbserver and susockets Socket.RemoteUser
	// only implemented in SocketConnectAsynch
	virtual char* getadr() = 0;

	Buffer rdbuf;
	Buffer wrbuf;
	};

// start a socket server (to listen)
// calls supplied newserver function for connections
typedef void (_stdcall *pNewServer)(void*);
void socketServer(char* title, int port, pNewServer newserver, void* arg, bool exit);

// create a synchronous (blocks everything) socket connection
SocketConnect* socketClientSynch(char* addr, int port, int timeout = 9999, int timeoutConnect = 0);

// create an asynch (only blocks calling fiber) socket connection
SocketConnect* socketClientAsynch(char* addr, int port);

SocketConnect* socketClientPoll(char* addr, int port, int timeout, int timeoutConnect);

int socketConnectionCount();

#endif
