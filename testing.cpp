// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "testing.h"
#include "exceptimp.h"
#include "fatal.h"
#include <span>
#include "ostreamstr.h"
#include "ostreamfile.h"
#include "alert.h"
#include "build.h"
#include "porttest.h"

// tests ------------------------------------------------------------

const int MAX_TESTS = 200;
static Test* tests[MAX_TESTS];
int n_tests = 0;

Test::Test(const char* n, Tfn f) : name(n), fn(f) {
	if (n_tests >= MAX_TESTS)
		fatal("too many TEST's - increase MAX_TESTS");
	tests[n_tests++] = this;
}

Testing::Testing()
	: log(*new OstreamFile("test.log", "w")), err(*new OstreamStr()) {
}

Testing::~Testing() {
	delete &log;
	delete &err;
}

bool run_tests(const char* prefix) {
	Testing t;
	for (auto test : std::span<Test*>(tests, n_tests))
		if (has_prefix(test->name, prefix)) {
			++t.ntests;
			t.log << test->name << endl;
			try {
				test->fn();
			} catch (const Except& e) {
				++t.nfails;
				t.log << e.str() << endl;
				t.err << test->name << " FAILED: " << e.str() << endl;
			}
		}
	PortTest::run(t, prefix);
	char* errs = (dynamic_cast<OstreamStr&>(t.err)).str();

	char* nt = OSTR(t.ntests << " test" << (t.ntests > 1 ? "s" : "") << ' ');
	if (t.nfails == 0)
		alert("Built: " << build << "\n\n"
						<< errs << endl
						<< nt << "ALL SUCCESSFUL");
	else
		alert("Built: " << build << "\n\n"
						<< errs << endl
						<< nt << t.nfails << " FAILURE"
						<< (t.nfails == 1 ? "" : "S"));
	return t.nfails == 0;
}

// benchmarks =======================================================

#include <chrono>

const int MAX_BENCHMARKS = 100;
Benchmark* benchmarks[MAX_BENCHMARKS];
int n_benchmarks = 0;

Benchmark::Benchmark(const char* n, Bfn f) : name(n), fn(f) {
	if (n_benchmarks >= MAX_BENCHMARKS)
		fatal("too many BENCHMARK's - increase MAX_BENCHMARKS");
	benchmarks[n_benchmarks++] = this;
}

// estimate how many reps per second
int64_t reps_per_sec(Bfn fn) {
	for (int64_t nreps = 1;; nreps *= 2) {
		auto t1 = std::chrono::high_resolution_clock::now();
		fn(nreps);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::nano> dur = t2 - t1;
		if (dur.count() > 20e6) // 20 ms in ns
			return 1e9 / dur.count() * nreps;
	}
}

void run_benchmark(Ostream& os, Benchmark* b) {
	os << "-bench " << b->name << ": ";
	try {
		int64_t nreps = reps_per_sec(b->fn);
		auto t1 = std::chrono::high_resolution_clock::now();
		b->fn(nreps);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::nano> dur = t2 - t1;
		os << lround(dur.count() / nreps) << " ns  ";
	} catch (const Except& e) {
		os << "failed: " << e;
	}
	os << endl;
}

void run_benchmarks(Ostream& os, const char* prefix) {
	for (auto b : std::span<Benchmark*>(benchmarks, n_benchmarks))
		if (has_prefix(b->name, prefix))
			run_benchmark(os, b);
}
