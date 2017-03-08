#ifndef GLOBALS_H
#define GLOBALS_H

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

#include "value.h"

typedef unsigned short ushort;

// global definitions - builtin and library
struct Globals
	{
	ushort operator()(const char* s);
	ushort copy(char* s);	// for class : _Base
	char* operator()(ushort j);
	Value operator[](const char* s);
	Value operator[](ushort j);
	Value find(const char* s)
		{ return find(operator()(s)); }
	Value find(ushort j);
	Value get(ushort j);
	Value get(const char* s)
		{ return get(operator()(s)); }
	void put(const char* s, Value x)
		{ put(operator()(s), x); }
	void put(ushort j, Value x);
	void clear();
	void pop(ushort i);
	};

extern Globals globals;

#endif
