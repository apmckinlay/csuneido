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
#include "exceptimp.h"
#include "fatal.h"
#include "gsl-lite.h"

// tests ------------------------------------------------------------

const int MAX_TESTS = 200;
static Test* tests[MAX_TESTS];
int n_tests = 0;

Test::Test(const char* n, Tfn f) : name(n), fn(f)
	{
	if (n_tests >= MAX_TESTS)
		fatal("too many TEST's - increase MAX_TESTS");
	tests[n_tests++] = this;
	}

int run_tests(TestObserver& to, const char* prefix)
	{
	int nfailed = 0;
	for (auto t : gsl::span<Test*>(tests, n_tests))
		if (has_prefix(t->name, prefix))
			{
			const char* error = nullptr;
			to.start_test(t->name);
			try
				{
				t->fn();
				}
			catch (const Except& e)
				{
				++nfailed;
				error = e.str();
				}
			to.end_test(t->name, error);
			}
	to.end_all(nfailed);
	return nfailed;
	}

// benchmarks =======================================================

#include "ostreamstr.h"
#include <chrono>

const int MAX_BENCHMARKS = 100;
Benchmark* benchmarks[MAX_BENCHMARKS];
int n_benchmarks = 0;

Benchmark::Benchmark(const char* n, Bfn f) : name(n), fn(f)
	{
	if (n_benchmarks >= MAX_BENCHMARKS)
		fatal("too many BENCHMARK's - increase MAX_BENCHMARKS");
	benchmarks[n_benchmarks++] = this;
	}

// estimate how many reps per second
long long reps_per_sec(Bfn fn)
	{
	for (long long nreps = 1; ; nreps *= 2)
		{
		auto t1 = std::chrono::high_resolution_clock::now();
		fn(nreps);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::nano> dur = t2 - t1;
		if (dur.count() > 20e6) // 20 ms in ns
			return 1e9 / dur.count() * nreps;
		}
	}

void run_benchmark(Ostream& os, Benchmark* b)
	{
	os << "-bench " << b->name << ": ";
	try
		{
		long long nreps = reps_per_sec(b->fn);
		auto t1 = std::chrono::high_resolution_clock::now();
		b->fn(nreps);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::nano> dur = t2 - t1;
		os << lround(dur.count() / nreps) << " ns  ";
		}
	catch (const Except& e)
		{
		os << "failed: " << e;
		}
	os << endl;
	}

void run_benchmarks(Ostream& os, const char* prefix)
	{
	for (auto b : gsl::span<Benchmark*>(benchmarks, n_benchmarks))
		if (has_prefix(b->name, prefix))
			run_benchmark(os, b);
	}
