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

#include "hashfn.h"

// TODO: check if it would be better to inline than unwind

size_t hashfn(const char* s)
	{
	// this is a version of hashpjw / elfhash
	// WARNING: assumes size_t is 32 bits
	// unwind the code to the point where the 32 bits are filled up
	size_t result = *s++;
	if (! *s)
		return result;
	result = (result << 4) + *s++;
	if (! *s)
		return result;
	result = (result << 4) + *s++;
	if (! *s)
		return result;
	result = (result << 4) + *s++;
	if (! *s)
		return result;
	result = (result << 4) + *s++;
	if (! *s)
		return result;
	result = (result << 4) + *s++;
	if (! *s)
		return result;
	// handle any remaining characters
	for (; *s; ++s)
		result = (((result << 4) + *s) ^ (result >> 24)) & 0xfffffff;
	return result;
	}

size_t hashfn(const char* s, int n)
	{
	// WARNING: assumes size_t is 32 bits
	// unwind the code to the point where the 32 bits are filled up
	if (n <= 0)
		return 0;
	size_t result = *s++;
	if (! --n)
		return result;
	result = (result << 4) + *s++;
	if (! --n)
		return result;
	result = (result << 4) + *s++;
	if (! --n)
		return result;
	result = (result << 4) + *s++;
	if (! --n)
		return result;
	result = (result << 4) + *s++;
	if (! --n)
		return result;
	result = (result << 4) + *s++;
	if (! --n)
		return result;
	// handle any remaining characters
	for (; n; ++s, --n)
		result = (((result << 4) + *s) ^ (result >> 24)) & 0xfffffff;
	return result;
	}
