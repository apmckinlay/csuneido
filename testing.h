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

#include "except.h"

// tests ------------------------------------------------------------

#define TEST(name) \
	static void test_##name(); \
	static Test testcase##name(#name, test_##name); \
	static void test_##name()

using Tfn = void(*)();

struct Test
	{
	Test(const char* name, Tfn f);

	const char* name;
	Tfn fn;
	};

bool run_tests(const char* prefix);

struct Testing
	{
	Testing();
	~Testing();

	Ostream& log;
	Ostream& err;
	int ntests = 0;
	int nfails = 0;
	};

// assertions -------------------------------------------------------

// WARNING: error message evaluates args a second time
#define assert_eq(x, y) except_if(! ((x) == (y)), \
	"error: " << #x << " != " << #y << " (" << (x) << " != " << (y) << ")")

// WARNING: error message evaluates args a second time
#define assert_streq(x, y) except_if(0 != strcmp((x), (y)), \
	"error: " << #x << " != " << #y << " (" << (x) << " != " << (y) << ")")

// WARNING: error message evaluates args a second time
#define assert_neq(x, y) except_if((x) == (y), \
	"error: " << #x << " == " << #y << " (" << (x) << " == " << (y) << ")")

#define xassert(expr) \
	do { \
	bool err = false; \
	try { expr; } catch (...) { err = true; } \
	if (!err) \
		except_err("error: expected exception from " #expr); \
	} while(false)

// benchmarks -------------------------------------------------------

using Bfn = void(*)(long long);

#define BENCHMARK(name) \
	void bench_##name(long long nreps); \
	static Benchmark benchmark##name(#name, bench_##name); \
	void bench_##name(long long nreps)

struct Benchmark
	{
	Benchmark(const char* n, Bfn f);

	const char* name;
	Bfn fn;
	};

void run_benchmarks(Ostream& os, const char* prefix);
