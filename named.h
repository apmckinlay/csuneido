#ifndef NAMED_H
#define NAMED_H

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

#include "gcstring.h"

struct Named
	{
	explicit Named(char* c = " ") : parent(0), separator(c), num(0), str(NULL)
		{ }
	gcstring name();
	gcstring library();
	gcstring info(); // for debugging

	gcstring lib;
	Named* parent;
	char* separator;
	ushort num;
	char* str; // alternative to num to avoid symbol
	};

// place NAMED in the public portion of a class to implement naming

#define NAMED \
	Named named; \
	virtual Named* get_named() \
		{ return &named; }

#endif
