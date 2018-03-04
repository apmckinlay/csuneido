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

#include "membase.h"

class SuObject;
class Ostream;

// user defined classes
class SuClass : public MemBase
	{
public:
	NAMED
	SuClass() : named("."), base(0)
		{ }
	explicit SuClass(short b) : named("."), base(b)
		{ }
	void out(Ostream&) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort *argnames, int each) override;
	Value getdata(Value) override;
	bool eq(const SuValue& x) const override
		{ return this == &x; }
	size_t hashcontrib() const override;

	bool hasMethod(Value mem)
		{ return !!method_class(mem); }
protected:
	virtual Value get(Value m) const; // overridden by SuSocketServer
private:
	void put(Value m, Value x); // used by compile
	Value get2(Value self, Value member);
	Value get3(Value member);
	Value mbclass() override
		{ return this; }
	Value parent() override;
	bool readonly() override
		{ return true; }

	const short base;
	bool has_getter = true;

	friend class SuInstance;
	friend struct ClassContainer;
	};
