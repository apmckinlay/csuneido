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

#include "value.h"
#include "membase.h"

class SuClass;

class SuInstance : public MemBase
	{
public:
	SuInstance(Value c) : myclass(c)
		{}

	Value getdata(Value) override;
	void putdata(Value, Value) override;
	Value call(Value self, Value member, short nargs,
		short nargnames, ushort* argnames, int each) override;
	bool eq(const SuValue& x) const override;
	size_t hashcontrib() const override;
	void out(Ostream&) const override;
	gcstring to_gcstr() const override;
	const char* type() const override;

private:
	Value Copy(short nargs, short nargnames, ushort* argnames, int each);
	Value Delete(short nargs, short nargnames, ushort* argnames, int each);
	const char* toString() const;

	Value mbclass() override
		{ return myclass; }
	Value parent() override
		{ return myclass; }
	bool readonly() override
		{ return false; }

	// instance references its class directly, not by global
	// partly to allow instances of anonymous classes
	const Value myclass;
	};

