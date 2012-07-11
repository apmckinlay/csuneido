#ifndef BUILTINARGS_H
#define BUILTINARGS_H

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
	void exceptUsage();
	Value getValue(char* name);
	Value getValue(char* name, Value defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val : defval;
		}
	int getint(char* name)
		{ ckndef(); return getValue(name).integer(); }
	int getint(char* name, int defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.integer() : defval;
		}
	char* getstr(char* name)
		{ ckndef(); return getValue(name).str(); }
	char* getstr(char* name, char* defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.str() : defval;
		}
	gcstring getgcstr(char* name)
		{ ckndef(); return getValue(name).gcstr(); }
	gcstring getgcstr(char* name, gcstring defval)
		{
		def = true;
		Value val = getval(name);
		return val ? val.gcstr() : defval;
		}
	bool hasNamed()
		{ return nargnames > 0; }
	Value getNext();
	ushort curName();
	Value getNextUnnamed();
	void end();
private:
	Value getval(char* name);
	void ckndef();
	Lisp<int> taken;

	short nargs;
	short nargnames;
	ushort* argnames;
	int unnamed;
	const char* msg1;
	const char* msg2;
	const char* msg3;
	int i;
	bool def;
	};

#endif
