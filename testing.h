#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "except.h"

// tests ------------------------------------------------------------

#define TEST(name) \
	void test_##name(); \
	static Test testcase##name(#name, test_##name); \
	void test_##name()

using Tfn = void (*)();

struct Test {
	Test(const char* name, Tfn f);

	const char* name;
	Tfn fn;
};

bool run_tests(const char* prefix);

struct Testing {
	Testing();
	~Testing();

	Ostream& log;
	Ostream& err;
	int ntests = 0;
	int nfails = 0;
};

// assertions -------------------------------------------------------

// WARNING: error message evaluates args a second time
#define assert_eq(x, y) \
	except_if(!((x) == (y)), \
		"error: " << #x << " != " << #y << " (" << (x) << " != " << (y) \
				  << ")")

// WARNING: error message evaluates args a second time
#define assert_streq(x, y) \
	except_if(0 != strcmp((x), (y)), \
		"error: " << #x << " != " << #y << " (" << (x) << " != " << (y) \
				  << ")")

// WARNING: error message evaluates args a second time
#define assert_neq(x, y) \
	except_if((x) == (y), \
		"error: " << #x << " == " << #y << " (" << (x) << " == " << (y) \
				  << ")")

#define xassert(expr) \
	do { \
		bool err = false; \
		try { \
			expr; \
		} catch (...) { \
			err = true; \
		} \
		if (!err) \
			except_err("error: expected exception from " #expr); \
	} while (false)

// benchmarks -------------------------------------------------------

using Bfn = void (*)(int64_t);

#define BENCHMARK(name) \
	void bench_##name(int64_t nreps); \
	static Benchmark benchmark##name(#name, bench_##name); \
	void bench_##name(int64_t nreps)

struct Benchmark {
	Benchmark(const char* n, Bfn f);

	const char* name;
	Bfn fn;
};

void run_benchmarks(Ostream& os, const char* prefix);
