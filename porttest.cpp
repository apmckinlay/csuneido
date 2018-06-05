/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2018 Suneido Software Corp.
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

#include "porttest.h"
#include "gcstring.h"
#include "except.h"
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

PortTest::PortTest(const char* n, PTfn f) : name(n), fn(f)
	{
	if (n_porttests >= MAX_PORTTEST)
		fatal("too many PORTTEST, increase MAX_PORTTEST");
	porttests[n_porttests++] = this;
	}

static PortTest* find(const char* name)
	{
	for (auto pt : gsl::span<PortTest*>(porttests, n_porttests))
		if (0 == strcmp(name, pt->name))
			return pt;
	return nullptr;
	}

// assumes suneido_tests is a peer of the current directory
#define TESTDIR "../suneido_tests/"

List<gcstring> test_files()
	{
	List<gcstring> files;
	struct _finddata_t f{};
	intptr_t h;
	if (-1L != (h = _findfirst(TESTDIR "*.test", &f)))
		do
			{
			files.add(gcstring(f.name));
			} while (_findnext(h, &f) == 0);
	_findclose(h);
	return files;
	}

long file_size(FILE* f)
	{
	fseek(f, 0, SEEK_END);
	auto n = ftell(f);
	fseek(f, 0, SEEK_SET);
	return n;
	}

char* read_file(gcstring file)
	{
	file = TESTDIR + file;
	FILE* f = fopen(file.str(), "r");
	if (!f)
		return nullptr;
	auto n = file_size(f);
	auto buf = new char[n+1];
	fread(buf, 1, n, f);
	buf[n] = 0;
	return buf;
	}

struct Parser
	{
	Parser(Testing& t_, const char* f, const char* s) 
		: filename(f), scan(s), t(t_)
		{
		next(true);
		}

	void run()
		{
		while (tok != Eof)
			run1();
		if (!missing.empty())
			{
			t.err << filename << " MISSING FIXTURES: ";
			t.log << filename << " MISSING FIXTURES: ";
			for (gcstring s : missing)
				{
				t.err << s << " ";
				t.log << s << " ";
				}
			t.err << endl;
			t.log << endl;
			}
		}

	void run1()
		{
		match('@', false);
		auto name = gcstring(scan.value);
		match(T_IDENTIFIER, true);
		PortTest* pt = find(name.str());
		if (pt)
			t.log << filename << ": @" << name << " " << comment << endl;
		else
			missing.add(name);
		int n = 0;
		while (tok != EOF && tok != '@') {
			List<const char*> args;
			List<bool> str;
			while (true) {
				const char* text = dupstr(scan.value);
				if (tok == I_SUB)
					{
					next(false);
					text = (gcstring("-") + scan.value).str();
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
				{
				if (char* errinfo = pt->fn(args, str))
					{
					++t.nfails;
					t.err << filename << ": @" << name << " " << comment << endl;
					t.err << "\tFAILED: " << args << " " << errinfo << endl;
					t.log << "\tFAILED: " << args << " " << errinfo << endl;
					}
				else
					n++;
				}
			next(true);
			}
		if (pt)
			t.log << "\t" << n << " passed" << endl;
		++t.ntests;
		}

	void match(int expected, bool skip)
		{
		if (tok != expected && scan.keyword != expected)
			except("porttests syntax error on " << scan.value);
		next(skip);
		}

	void next(bool skip) 
		{
		comment = "";
		bool nl = false;
		while (true) {
			tok = scan.nextall();
			switch (tok)
				{
			case T_NEWLINE:
				if (!skip)
					return;
				nl = true;
			case T_WHITE:
				continue;
			case T_COMMENT:
				if (!nl)
					comment = gcstring(scan.source + scan.prev,
						scan.si - scan.prev);
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
	};

static void run_file(Testing& t, const char* filename)
	{
	Parser parser(t, filename, read_file(filename));
	parser.run();
	}

#include "ostreamstr.h"

void PortTest::run(Testing& t)
	{
	OstreamStr os;
	for (auto f : test_files())
		run_file(t, f.str());
	}
