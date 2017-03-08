#ifndef SUFUNCTION_H
#define SUFUNCTION_H

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

#include "func.h"

// debug information for compiled SuFunction's
struct Debug
	{
	int si;
	int ci;
	Debug()
		{ }
	Debug(int s, int c) : si(s), ci(c)
		{ }
	};

// user defined, interpreted functions
class SuFunction : public Func
	{
public:
	SuFunction()
		: code(0), nc(0), db(0), nd(0), nlocals(0), nliterals(0), src("")
		{ }

	uchar* code;
	int nc;
	Debug* db;
	short nd;
	short nlocals;
	short nliterals;
	const char* src;
	char* className; // for dot parameters, used by SuFunction call

	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);

	virtual size_t packsize() const;
	virtual void pack(char* buf) const;
	static SuFunction* unpack(const gcstring& s);

	void source(Ostream&, int ci);
	int source(int ci, int* pn);
	void disasm(Ostream&);
	int disasm1(Ostream&, int);

private:
	void dotParams(Value self);
	char* mem(int& ci);
	};

#endif
