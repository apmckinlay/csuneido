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

#include "suobject.h"
#include "named.h"
#include "globals.h"

class Ostream;

class Get2
	{
protected:
	~Get2() = default;
public:
	virtual Value get2(Value self, Value member) = 0;
	};

// user defined classes
class SuClass : public SuObject, public Get2
	{
public:
	NAMED
	explicit SuClass(short b) : named("."), base(b)
		{ }
	void out(Ostream&) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort *argnames, int each) override;
	Value getdata(Value) override;
	Value get2(Value self, Value member) override;
	bool eq(const SuValue& x) const override;

private:
	Value get3(Value member);

public:
	short base;
private:
	bool has_getter = true;
	};

// conceptually RootClass is a kind of Class
// but practically, it doesn't need to derive from SuClass
// since it never has any members
class RootClass : public SuValue, public Get2
	{
public :
	void out(Ostream& os) const override
		{ os << "Object"; }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort *argnames, int each) override;
	Value getdata(Value) override
		{ return Value(); }
	Value get2(Value self, Value member) override
		{ return Value(); }
	static Value notfound(Value self, Value member,
		short nargs, short nargnames, ushort *argnames, int each);
	};
