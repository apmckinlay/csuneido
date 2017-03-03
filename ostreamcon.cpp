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

#include "ostreamcon.h"
#include "win.h"

class OstreamConImp
	{
public:
	OstreamConImp()
		{
		AllocConsole();
		con = GetStdHandle(STD_OUTPUT_HANDLE);
		}
	void close()
		{ FreeConsole(); }
	void add(const void* s, int n)
		{
		DWORD nw;
		WriteFile(con, s, n, &nw, NULL);
		}
	explicit operator bool() const
		{ return con; }
private:
	HANDLE con;
	};

OstreamCon::OstreamCon() 
	: imp(new OstreamConImp)
	{ }

OstreamCon::~OstreamCon()
	{ imp->close(); }

Ostream& OstreamCon::write(const void* s, int n)
	{
	imp->add(s, n);
	return *this;
	}

OstreamCon::operator bool() const
	{
	return bool(*imp);
	}

Ostream& con()
	{
	static OstreamCon con;
	return con;
	}