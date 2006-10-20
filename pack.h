#ifndef PACK_H
#define PACK_H

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

#include <stddef.h>

class Value;
class gcstring;

Value unpack(const gcstring& s);
gcstring unpack_gcstr(const gcstring& s);

int packvalue(char* buf, Value x); 
Value unpackvalue(const char*& buf);

size_t packsize(long n);
void packlong(char* buf, long n);
long unpacklong(const gcstring& s);

int packstrsize(const char* s);
int packstr(char* buf, const char* s);
const char* unpackstr(const char*& buf);

struct Named;
int packnamesize(const Named& named);
int packname(char* buf, const Named& named);
int unpackname(const char* buf, Named& named);

// in sort order
enum
	{ 
	PACK_FALSE, PACK_TRUE,
	PACK_MINUS, PACK_PLUS, 
	PACK_STRING, 
	PACK_DATE, 
	PACK_OBJECT,
	PACK_RECORD,
	PACK_FUNCTION,
	PACK_CLASS
	};

#endif
