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

// ReSharper disable once CppUnusedIncludeDirective
#include <stddef.h> // for size_t

class Value;
class gcstring;

Value unpack(const char* buf, int len);
Value unpack(const gcstring& s);
gcstring unpack_gcstr(const gcstring& s);

size_t packintsize(int n);
void packint(char* buf, int n);
int unpackint(const gcstring& s);

int packstrsize(const char* s);
int packstr(char* buf, const char* s);
const char* unpackstr(const char*& buf);

// in sort order
enum
	{
	PACK_FALSE, PACK_TRUE,
	PACK_MINUS, PACK_PLUS,
	PACK_STRING,
	PACK_DATE,
	PACK_OBJECT,
	PACK_RECORD
	};

// new version
enum
	{
	PACK2_FALSE, PACK2_TRUE,
	PACK2_NEG_INF, PACK2_NEG, PACK2_ZERO, PACK2_POS, PACK2_POS_INF, // number
	PACK2_STRING,
	PACK2_DATE,
	PACK2_OBJECT,
	PACK2_RECORD
	};
