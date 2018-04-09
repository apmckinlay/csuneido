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

#include "cvt.h"

// complement leading bit to ensure correct unsigned compare

char* cvt_long(char* p, long n)
	{
	n ^= 0x80000000;
	p[3] = (char) n;
	p[2] = (char) (n >>= 8);
	p[1] = (char) (n >>= 8);
	p[0] = (char) (n >> 8);
	return p;
	}

long cvt_long(const char* q)
	{
	const unsigned char* p = (unsigned char*) q;
	long n = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	return n ^ 0x80000000;
	}

char* cvt_short(char* p, short n)
	{
	p[1] = (char) n;
	p[0] = (char) (n >> 8);
	return p;
	}

short cvt_short(const char* q)
	{
	const unsigned char* p = (unsigned char*) q;
	return (p[0] << 8) | p[1];
	}

#include "testing.h"

TEST(cvt_short)
	{
	char buf[2];
	assert_eq(0, cvt_short(cvt_short(buf, 0)));
	assert_eq(1234, cvt_short(cvt_short(buf, 1234)));
	assert_eq(-1234, cvt_short(cvt_short(buf, -1234)));
	}

TEST(cvt_long)
	{
	char buf[4];
	assert_eq(0, cvt_long(cvt_long(buf, 0)));
	assert_eq(1234, cvt_long(cvt_long(buf, 1234)));
	assert_eq(-1234, cvt_long(cvt_long(buf, -1234)));
	assert_eq(12345678, cvt_long(cvt_long(buf, 12345678)));
	assert_eq(-12345678, cvt_long(cvt_long(buf, -12345678)));
	}
