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

// the server side of the *binary* client-server protocol

#include "clientserver.h"
#include "connection.h"
#include "dbms.h"

typedef  void (*CmdFn)(Connection& io);

static CmdFn commands[(int) Command::N_COMMANDS];

// helper to make it easier to declare the commands and add them to the array
struct CmdHelper
	{
	CmdHelper(int ci, CmdFn fn)
		{ commands[ci] = fn; }
	};
#define COMMAND(cmd) \
static void cmd_##cmd(Connection&); \
static CmdHelper cmdHelper_##cmd((int) Command::cmd, cmd_##cmd); \
static void cmd_##cmd(Connection& io)

COMMAND(ADMIN)
	{
	}

COMMAND(AUTH)
	{
	}

COMMAND(CHECK)
	{
	}

COMMAND(CLOSE)
	{
	}

COMMAND(COMMIT)
	{
	}

COMMAND(CONNECTIONS)
	{
	}

COMMAND(CURSOR)
	{
	}

COMMAND(CURSORS)
	{
	}

COMMAND(DUMP)
	{
	}

COMMAND(ERASE)
	{
	}

COMMAND(EXEC)
	{
	}

COMMAND(EXPLAIN)
	{
	}

COMMAND(FINAL)
	{
	}

COMMAND(GET)
	{
	}

COMMAND(GET1)
	{
	}

COMMAND(HEADER)
	{
	}

COMMAND(KEYS)
	{
	}

COMMAND(KILL)
	{
	}

COMMAND(LIBGET)
	{
	}

COMMAND(LIBRARIES)
	{
	}

COMMAND(LOAD)
	{
	}

COMMAND(LOG)
	{
	}

COMMAND(NONCE)
	{
	}

COMMAND(ORDER)
	{
	}

COMMAND(OUTPUT)
	{
	}

COMMAND(QUERY)
	{
	}

COMMAND(READCOUNT)
	{
	}

COMMAND(REQUEST)
	{
	}

COMMAND(REWIND)
	{
	}

COMMAND(RUN)
	{
	}

COMMAND(SESSIONID)
	{
	}

COMMAND(SIZE)
	{
	}

COMMAND(TIMESTAMP)
	{
	Value result = dbms()->timestamp();
	io.put(true).putValue(result);
	}

COMMAND(TOKEN)
	{
	}

COMMAND(TRANSACTION)
	{
	}

COMMAND(TRANSACTIONS)
	{
	}

COMMAND(UPDATE)
	{
	}

COMMAND(WRITECOUNT)
	{
	}
