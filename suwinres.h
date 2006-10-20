#ifndef SUWINRES_H
#define SUWINRES_H

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

#include "sufinalize.h"

// base for Windows resources - SuHandle and SuGdiObj
class SuWinRes : public SuFinalize
	{
public:
	SuWinRes(void* handle);
	virtual Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each);
	virtual void finalize();
	void* handle()
		{ return h; }
protected:
	virtual bool close() = 0;
	void* h;
	};

// a value type to hold Windows HANDLE's
class SuHandle : public SuWinRes
	{
public:
	SuHandle(void* handle);
	virtual void out(Ostream& os);
	virtual bool close();
	};

// a value type to hold Windows GDIOBJ's
class SuGdiObj : public SuWinRes
	{
public:
	SuGdiObj(void* handle);
	virtual void out(Ostream& os);
	virtual int integer() const;
	virtual bool close();
	};

#endif
