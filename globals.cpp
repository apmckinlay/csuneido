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

#include "globals.h"
#include "library.h"
#include "std.h"
#include "hashmap.h"
#include <vector>
#include "permheap.h"

Globals globals;

const int INITSIZE = 500;
const int AVG_NAME_LEN = 12;
const int MAXNAMES = AVG_NAME_LEN * 32 * 1024;

#define MISSING	((SuValue*) 1)

static HashMap<const char*, int> tbl(INITSIZE);
static std::vector<Value> data;
static std::vector<char*> names;
static PermanentHeap ph("global names", MAXNAMES);

char* Globals::operator()(ushort j)
	{
	return names[j];
	}

Value Globals::operator[](const char* s)
	{
	return operator[](operator()(s));
	}

Value Globals::get(ushort j)
	{
	return data[j].ptr() == MISSING ? Value() : data[j];
	}

void Globals::put(ushort j, Value x)
	{
	data[j] = x;
	}

void Globals::clear()
	{
	std::fill(data.begin(), data.end(), Value());
	}

ushort Globals::operator()(const char* s)
	{
	if (int* pi = tbl.find(s))
		return *pi;
	char* str = strcpy((char*) ph.alloc(strlen(s) + 1), s);
	ushort num = (ushort) names.size();
	names.push_back(str);
	verify(data.size() <= USHRT_MAX);
	data.push_back(Value());
	tbl[str] = num;
	return num;
	}

Value Globals::operator[](ushort j)
	{
	if (Value x = find(j))
		return x;
	else
		except("can't find " << names[j]);
	}

Value Globals::find(ushort j)
	{
	Value x = data[j];
	if (x.ptr() == MISSING)
		return Value();

	if (! x)
		{
		libload(j);
		x = data[j];
		if (! x)
			data[j] = MISSING;
		}
	return x;
	}

ushort Globals::copy(char* s)	// called by Compiler::suclass for class : _Base
	{
	Value x(get(s + 1));
	if (! x)
		except("can't find " << s);
	names.push_back(strcpy((char*) ph.alloc(strlen(s) + 1), s));
	data.push_back(x);
	return (ushort) data.size() - 1;
	}

#include "testing.h"
#include "random.h"

class test_globals : public Tests
	{
	TEST(1, global)
		{
		char buf[512];
		const int N = 6400;
		int nums[N];

		srand(1234);
		int i;
		for (i = 0; i < N; ++i)
			{
			nums[i] = globals(randname(buf));
			verify(0 == strcmp(buf, globals(nums[i])));
			}

		srand(1234);
		for (i = 0; i < N; ++i)
			{
			verify(nums[i] == globals(randname(buf)));
			verify(0 == strcmp(buf, globals(nums[i])));
			}
		}
	char* randname(char* buf)
		{
		const int N = 8;
		char* dst = buf;
		int n = random(N);
		for (int j = 0; j <= n; ++j)
			*dst++ = 'a' + random(26);
		*dst = 0;
		return buf;
		}
	};
REGISTER(test_globals);
