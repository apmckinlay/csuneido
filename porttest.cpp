// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "porttest.h"
#include "gcstring.h"
#include "exceptimp.h"
#include <cstdio>
#include <io.h>
#include "list.h"
#include "scanner.h"
#include "opcodes.h"
#include "htbl.h"
#include "fatal.h"

const int MAX_PORTTEST = 20;
static PortTest* porttests[MAX_PORTTEST];
static int n_porttests = 0;

PortTest::PortTest(const char* n, PTfn f) : name(n), fn(f) {
	if (n_porttests >= MAX_PORTTEST)
		fatal("too many PORTTEST, increase MAX_PORTTEST");
	porttests[n_porttests++] = this;
}

static PortTest* find(const char* name) {
	for (auto pt : gsl::span<PortTest*>(porttests, n_porttests))
		if (0 == strcmp(name, pt->name))
			return pt;
	return nullptr;
}

// assumes suneido_tests is a peer of the current directory
#define TESTDIR "../suneido_tests/"

List<gcstring> test_files() {
	List<gcstring> files;
	struct _finddata_t f {};
	intptr_t h;
	if (-1L != (h = _findfirst(TESTDIR "*.test", &f)))
		do {
			files.add(gcstring(f.name));
		} while (_findnext(h, &f) == 0);
	_findclose(h);
	return files;
}

int file_size(FILE* f) {
	fseek(f, 0, SEEK_END);
	auto n = ftell(f);
	fseek(f, 0, SEEK_SET);
	return n;
}

char* read_file(gcstring file) {
	file = TESTDIR + file;
	FILE* f = fopen(file.str(), "r");
	if (!f)
		return nullptr;
	auto n = file_size(f);
	auto buf = new char[n + 1];
	fread(buf, 1, n, f);
	buf[n] = 0;
	return buf;
}

struct Parser {
	Parser(Testing& t_, const char* f, const char* s)
		: filename(f), scan(s), t(t_) {
		next(true);
	}

	void run() {
		while (tok != Eof)
			run_fixture();
		if (!missing.empty()) {
			t.err << filename << " MISSING FIXTURES: ";
			t.log << filename << " MISSING FIXTURES: ";
			for (gcstring s : missing) {
				t.err << s << " ";
				t.log << s << " ";
			}
			t.err << endl;
			t.log << endl;
		}
	}

	void run_fixture() {
		match('@', false);
		auto name = gcstring(scan.value);
		match(T_IDENTIFIER, true);
		PortTest* pt = find(name.str());
		if (pt)
			t.log << filename << ": @" << name << " " << comment << endl;
		else
			missing.add(name);
		n = 0;
		first = true;
		while (tok != EOF && tok != '@') {
			args.clear();
			str.clear();
			while (true) {
				gcstring text = gcstring(scan.value, scan.len);
				if (tok == I_SUB) {
					next(false);
					text = gcstring("-") + scan.value;
				}
				args.add(text);
				str.add(tok == T_STRING);
				next(false);
				if (tok == ',')
					next(true);
				if (tok == EOF || tok == T_NEWLINE)
					break;
			}
			if (pt)
				run_case(name, pt);
			next(true);
		}
		if (pt)
			t.log << "\t" << n << " passed" << endl;
		++t.ntests;
	}

	void run_case(gcstring name, PortTest* pt) {
		const char* errinfo;
		try {
			errinfo = pt->fn(args, str);
		} catch (const Except& e) {
			errinfo = ("\n\tthrew " + e.gcstr()).str();
		}
		if (errinfo) {
			++t.nfails;
			if (first)
				t.err << filename << ": @" << name << endl;
			first = false;
			t.err << "\tFAILED: " << args << " " << errinfo << endl;
			t.log << "\tFAILED: " << args << " " << errinfo << endl;
		} else
			n++;
	}

	void match(int expected, bool skip) {
		if (tok != expected && scan.keyword != expected)
			except("porttests syntax error on " << scan.value);
		next(skip);
	}

	void next(bool skip) {
		comment = "";
		bool nl = false;
		while (true) {
			tok = scan.nextall();
			switch (tok) {
			case T_NEWLINE:
				if (!skip)
					return;
				nl = true;
			case T_WHITE:
				continue;
			case T_COMMENT:
				if (!nl)
					comment =
						gcstring(scan.source + scan.prev, scan.si - scan.prev);
				continue;
			default:
				return;
			}
		}
	}

	const char* filename;
	Scanner scan;
	int tok = 0;
	Testing& t;
	gcstring comment = "";
	ListSet<gcstring> missing;
	bool first = true;
	List<gcstring> args;
	List<bool> str;
	int n = 0;
};

static void run_file(Testing& t, const char* filename) {
	Parser parser(t, filename, read_file(filename));
	parser.run();
}

#include "ostreamstr.h"

void PortTest::run(Testing& t, const char* prefix) {
	OstreamStr os;
	for (auto f : test_files())
		if (f.has_prefix(prefix))
			run_file(t, f.str());
}

PORTTEST(ptest) {
	return args[0] == args[1] ? nullptr : "";
}
