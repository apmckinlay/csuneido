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

class BuiltinArgs
	{
public:
	BuiltinArgs(short n, short nn, ushort *an, int each);
	void usage(char* m)
		{ msg = m; }
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
	void end();
private:
	Value getval(char* name);
	void ckndef();

	short nargs;
	short nargnames;
	ushort* argnames;
	int unnamed;
	char* msg;
	int i;
	bool def;
	};

#endif
