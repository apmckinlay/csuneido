#pragma once
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

#include "suvalue.h"

// base for Windows resources - SuHandle and SuGdiObj
class SuWinRes : public SuValue
	{
public:
	explicit SuWinRes(void* handle);
	Value call(Value self, Value member,
		short nargs, short nargnames, ushort* argnames, int each) override;
	void* handle() const
		{ return h; }
protected:
	virtual bool close() = 0;
	void* h;
	};

// a value type to hold Windows HANDLE's
class SuHandle : public SuWinRes
	{
public:
	explicit SuHandle(void* handle);
	void out(Ostream& os) const override;
	bool close() override;
	};

// a value type to hold Windows GDIOBJ's
class SuGdiObj : public SuWinRes
	{
public:
	explicit SuGdiObj(void* handle);
	void out(Ostream& os) const override;
	int integer() const override;
	bool close() override;
	};
