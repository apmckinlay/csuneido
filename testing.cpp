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

#include "testing.h"
#include <string.h>
#include "except.h"

#define RUN(i) \
	if (testname##i()) \
		{ \
		char* error = 0; \
		to.start_test(group, testname##i()); \
		try \
			{ test##i(); } \
		catch (const Except& x) \
			{ ++nfailed; error = x.exception; } \
		to.end_test(group, testname##i(), error); \
		}

int Tests::runtests(TestObserver& to)
	{
	to.start_group(group);
	int nfailed = 0;
	RUN(0); RUN(1); RUN(2); RUN(3); RUN(4); RUN(5);
	RUN(6); RUN(7); RUN(8); RUN(9); RUN(10);
	to.end_group(group, nfailed);
	return nfailed;
	}

const int MAXTESTS = 100;
static TestRegister* tests[MAXTESTS];
static int ntests = 0;
static const char* duplicate = 0;
TestRegister::TestRegister(const char* n, Tests* (*f)()) : name(n), makefn(f)
	{
	verify(ntests + 1 < MAXTESTS);
	if (findtest(name))
		duplicate = name;
	tests[ntests++] = this;
	}
/*
static void failures(Ostream& os, int n)
	{
	if (n == 0)
		os << "SUCCESSFUL";
	else
		{
		os << n << " FAILURE";
		if (n > 1)
			os << "S";
		}
	os << endl;
	}

static void showdups(Ostream& os)
	{
	if (duplicate)
		os << "DUPLICATE TEST NAME: " << duplicate << endl << endl;
	}
*/
int TestRegister::runall(TestObserver& to)
	{
	int nfailed = 0;
//	showdups(results);
	for (int i = 0; i < ntests; ++i)
		nfailed += tests[i]->makefn()->runtests(to);
	to.end_all(nfailed);
	return nfailed;
	}

int TestRegister::runtest(const char* name, TestObserver& to)
	{
//	showdups(results);
	if (TestRegister* tr = findtest(name))
		{
		int nfailed = tr->makefn()->runtests(to);
		to.end_all(nfailed);
		return nfailed;
		}
//	else
//		results << "TEST NOT FOUND: " << name << endl;
	return -1;
	}

TestRegister* TestRegister::findtest(const char* name)
	{
	for (int i = 0; i < ntests; ++i)
		if (0 == strcmp(name, tests[i]->name))
			return tests[i];
	return 0;
	}

// test the testing framework

#ifdef STANDALONE

#define verify(e) if (! (e)) throw Except(#e, __FILE__, __LINE__); else

class SomeTests : public Tests
	{
public:
	TEST(1, first)
		{ }
	TEST(2, second)
		{ }
	};
REGISTER(SomeTests);

class OtherTests : public Tests
	{
public:
	TEST(1, first)
		{ verify(1 == 2); }
	TEST(2, second)
		{ }
	};
REGISTER(OtherTests);

class MyTests : public Tests
	{
public:
	TESTS(1, SomeTests);
	TESTS(2, OtherTests);
	};
REGISTER(MyTests);

class MyTests2 : public Tests
	{
public:
	TESTS(1, MyTests);
	TESTS(2, SomeTests);
	};
REGISTER(MyTests2);

void main()
	{
	TestRegister::run("MyTests");
	cerr << endl;
	}

#endif
