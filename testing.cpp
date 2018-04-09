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
#include <cstring>
#include "exceptimp.h"

// tests ------------------------------------------------------------

const int MAX_TESTS = 200;
static Test* newtests[MAX_TESTS];
int n_tests = 0;

Test::Test(const char* n, Tfn f) : name(n), fn(f)
	{
	if (n_tests < MAX_TESTS)
		newtests[n_tests++] = this;
	}

int run_tests(TestObserver& to, const char* prefix)
	{
	int nfailed = 0;
	for (int i = 0; i < n_tests; ++i)
		if (has_prefix(newtests[i]->name, prefix))
			{
			const char* error = nullptr;
			to.start_test(newtests[i]->name);
			try
				{
				newtests[i]->fn();
				}
			catch (const Except& e)
				{
				++nfailed;
				error = e.str();
				}
			to.end_test(newtests[i]->name, error);
			}
	if (n_tests >= MAX_TESTS)
		{
		to.end_test("", "MAX_TESTS NOT LARGE ENOUGH");
		++nfailed;
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
	verify(n_benchmarks < MAX_BENCHMARKS);
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
		os << int(dur.count() / nreps + .5) << " ns  ";
		}
	catch (const Except& e)
		{
		os << "failed: " << e;
		}
	os << endl;
	}

void run_benchmarks(Ostream& os, const char* prefix)
	{
	for (int i = 0; i < n_benchmarks; ++i)
		if (has_prefix(benchmarks[i]->name, prefix))
			run_benchmark(os, benchmarks[i]);
	}
