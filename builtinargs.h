#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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
#include "gcstring.h"
#include "lisp.h"

class BuiltinArgs
	{
public:
	BuiltinArgs(short& n, short& nn, ushort*& an, int& each);
	BuiltinArgs& usage(const char* s1, const char* s2 = "", const char* s3 = "")
		{ msg1 = s1; msg2 = s2; msg3 = s3; return *this; }
	void exceptUsage() const;
	Value getValue(const char* name);
	Value getValue(const char* name, Value defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val : defval;
		}
	int getint(const char* name)
		{ ckndef(); return getValue(name).integer(); }
	int getint(const char* name, int defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.integer() : defval;
		}
	const char* getstr(const char* name)
		{ ckndef(); return getValue(name).str(); }
	const char* getstr(const char* name, const char* defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.str() : defval;
		}
	gcstring getgcstr(const char* name)
		{ ckndef(); return getValue(name).gcstr(); }
	gcstring getgcstr(const char* name, gcstring defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.gcstr() : defval;
		}
	Value getNamed(const char* name);
	bool hasNamed() const
		{ return nargnames > 0; }
	Value getNext();
	ushort curName() const;
	Value getNextUnnamed();
	void end();
	bool hasUnnamed() const
		{ return unnamed > 0; }
	Value call(Value fn, Value self, Value method);
private:
	Value getval(const char* name);
	void ckndef() const;

	short nargs;
	short nargnames;
	ushort* argnames;
	int unnamed;
	const char* msg1 = "invalid arguments";
	const char* msg2 = "";
	const char* msg3 = "";
	int i = 0;
	bool def = false;
	Lisp<int> taken;
	};
