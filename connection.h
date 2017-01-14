#pragma once
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
	void write(char * buf, int n);
	void close();

private:
	SocketConnect& sc;
	Buffer rdbuf; // can't use sc.rdbuf because of how sc uses it
};
