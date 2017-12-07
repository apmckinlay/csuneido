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

#include "func.h"

// debug information for compiled SuFunction's
struct Debug
	{
	Debug(int s, int c) : si(s), ci(c)
		{ }

	int si;
	int ci;
	};

// user defined, interpreted functions
class SuFunction : public Func
	{
public:
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;

	void source(Ostream&, int ci);
	int source(int ci, int* pn);
	void disasm(Ostream&);
	int disasm1(Ostream&, int);

	uchar* code = nullptr;
	int nc = 0;
	Debug* db = nullptr;
	short nd = 0;
	short nlocals = 0;
	short nliterals = 0;
	const char* src = "";
	const char* className = nullptr; // for dot parameters, used by SuFunction call
private:
	void dotParams(Value self);
	const char* mem(int& ci);
	};
