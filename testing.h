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

class TestObserver
	{
public:
	virtual ~TestObserver() = default;
	virtual void start_group(const char* group)
		{ }
	virtual void start_test(const char* group, const char* test)
		{ }
	virtual void end_test(const char* group, const char* test, const char* error)
		{ }
	virtual void end_group(const char* group, int nfailed)
		{ }
	virtual void end_all(int nfailed)
		{ }
	};

#define BASE(i) \
	virtual const char* testname##i() { return 0; } \
	virtual void test##i() { }

class Tests
	{
public:
	virtual ~Tests() = default;
	int runtests(TestObserver&);
	BASE(0) BASE(1) BASE(2) BASE(3) BASE(4) BASE(5)
	BASE(6) BASE(7) BASE(8) BASE(9) BASE(10)
	const char* group;
	};

#define TEST(i,name) \
	const char* testname##i() override { return #name; } \
	void test##i() override

#define TESTS(i,name) \
	const char* testname##i() override { return #name; } \
	void test##i() override { name t; t.group = #name; t.run(); }

class TestRegister
	{
public:
	TestRegister(const char* n, Tests* (*f)());
	static int runtest(const char* name, TestObserver&);
	static int runall(TestObserver&);
private:
	static TestRegister* findtest(const char* name);
	const char* name;
	Tests* (*makefn)();
	};

#define REGISTER(name) \
	static Tests* make_##name() { auto t = new name; t->group = #name; return t; } \
	static TestRegister register_##name(#name, make_##name)

// ReSharper disable once CppUnusedIncludeDirective
#include "except.h"

#define assert_eq(x, y) \
	do { \
	auto xx = x; \
	auto yy = y; \
	except_if(! (xx == yy), \
		"error: " << #x << " != " << #y << " (" << xx << " != " << yy << ")"); \
	} while (false)

#define assert_neq(x, y) \
	do { \
	auto xx = x; \
	auto yy = y; \
	except_if(xx == yy, \
		"error: " << #x << " == " << #y << " (" << xx << " == " << yy << ")"); \
	} while (false)

#define xassert(expr) \
	do { \
	bool err = false; \
	try { expr; } catch (...) { err = true; } \
	if (!err) \
		except_err("error: expected exception from " #expr); \
	} while(false)
