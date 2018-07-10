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

#include "meth.h"
#include "named.h"
#include "value.h"
#include "htbl.h"

class MemBase : public SuValue
	{
public:
	NAMED
	MemBase() : named(".")
		{ }

private:
	template <typename Finder> Value lookup(Finder finder);
	void addMembersTo(SuObject* ob);
	Value method_lookup(BuiltinArgs& args);

protected:
	Value Base(BuiltinArgs& args);
	bool hasBase(Value value);
	Value BaseQ(BuiltinArgs& args);
	Value Eval(BuiltinArgs& args);
	Value Eval2(BuiltinArgs& args);
	Value GetDefault(BuiltinArgs& args);
	Value MemberQ(BuiltinArgs& args);
	Value Members(BuiltinArgs& args);
	Value MethodQ(BuiltinArgs& args);
	Value MethodClass(BuiltinArgs& args);
	virtual Value ReadonlyQ(BuiltinArgs& args);
	Value Size(BuiltinArgs& args);

	Value method_class(Value m);
	static MemFun<MemBase> method(Value member);
	Value callSuper(Value self, Value member, short nargs, short nargnames, uint16_t* argnames, int each);
	virtual Value mbclass() = 0; // SuInstance.myclass or SuClass this
	virtual Value parent() = 0; // SuInstance.myclass or SuClass.base
	virtual bool readonly() = 0;

	Hmap<Value, Value> data;
	};
