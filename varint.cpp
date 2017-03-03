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

// NOTE: only for positive numbers (doesn't zigzag encode)

#include "varint.h" 
#include "except.h"

void push_varint(vector<uchar>& code, int n)
	{
	verify(n >= 0);
	while (n > 0x7f)
		{
		code.push_back((n & 0x7f) | 0x80);
		n >>= 7;
		}
	code.push_back(n & 0x7f);
	}

int varint(uchar* code, int& ci)
	{
	int shift = 0;
	int n = 0;
	int b;
	do
		{
		b = code[ci++];
		n |= (b & 0x7f) << shift;
		shift += 7;
		} while (b & 0x80);
	return n;
	}

int varint(uchar*& code)
	{
	int shift = 0;
	int n = 0;
	int b;
	do
		{
		b = *code++;
		n |= (b & 0x7f) << shift;
		shift += 7;
		} while (b & 0x80);
	return n;
	}

#include "testing.h"

class test_varint : public Tests
	{
	TEST(0, main)
		{
		test(0);
		test(99);
		test(1234);
		test(12345);
		}
	void test(int n)
		{
		vector<uchar> code;
		push_varint(code, n);
		int ci = 0;
		assert_eq(varint(&code[0], ci), n);
		}
	};
REGISTER(test_varint);
