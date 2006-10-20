#ifndef TESTING_H
#define TESTING_H

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

class TestObserver
	{
public:
	virtual ~TestObserver()
		{ }
	virtual void start_group(char* group)
		{ }
	virtual void start_test(char* group, char* test)
		{ }
	virtual void end_test(char* group, char* test, char* error)
		{ }
	virtual void end_group(char* group, int nfailed)
		{ }
	virtual void end_all(int nfailed)
		{ }
	};

#define BASE(i) \
	virtual char* testname##i() { return 0; } \
	virtual void test##i() { }

class Tests
	{
public:
	virtual ~Tests()
		{ }
	int runtests(TestObserver&);
	BASE(0) BASE(1) BASE(2) BASE(3) BASE(4) BASE(5)
	BASE(6) BASE(7) BASE(8) BASE(9) BASE(10)
	char* group;
	};

#define TEST(i,name) \
	char* testname##i() { return #name; } \
	void test##i()

#define TESTS(i,name) \
	char* testname##i() { return #name; } \
	void test##i() { name t; t.group = #name; t.run(); }

class TestRegister
	{
public:
	TestRegister(char* n, Tests* (*f)());
	static int runtest(char* name, TestObserver&);
	static int runall(TestObserver&);
private:
	static TestRegister* findtest(char* name);
	char* name;
	Tests* (*makefn)();
	};

#define REGISTER(name) \
	static Tests* make_##name() { name* t = new name; t->group = #name; return t; } \
	static TestRegister register_##name(#name, make_##name)

#include "except.h"

#define assert_eq(x, y)	except_if((x) != (y), "error: " << #x << " != " << #y << " (" << (x) << " != " << (y) << ")")

#define xassert(expr) \
	{ \
	bool err = false; \
	try { expr; } catch (...) { err = true; } \
	verify(err); \
	}

#endif
