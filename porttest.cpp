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
	Parser(Ostream& o, const char* s) : scan(s), os(o)
		{
		next(true);
		}
	bool run() {
		bool ok = true;
		while (tok != Eof)
			ok = run1() && ok;
		return ok;
		}

	bool run1()
		{
		match('@', false);
		auto name = dupstr(scan.value);
		match(T_IDENTIFIER, true);
		os << "@" << name << ":" << endl;
		PortTest* pt = find(name);
		int n = 0;
		bool ok = true;
		while (tok != EOF && tok != '@') {
			List<const char*> args;
			while (true) {
				const char* text = dupstr(scan.value);
				if (tok == I_SUB)
					{
					next(false);
					text = (gcstring("-") + scan.value).str();
					}
				args.add(text);
				next(false);
				if (tok == ',')
					next(true);
				if (tok == EOF || tok == T_NEWLINE)
					break;
				}
			if (!pt)
				{
				ok = false;
				os << "\tMISSING TEST FIXTURE" << endl;
				}
			else if (!pt->fn(args, os))
				{
				ok = false;
				os << "\tFAILED: " << args << endl;
				}
			else
				n++;
			next(true);
			}
		if (pt)
			os << "\t" << n << " passed" << endl;
		return ok;
		}

	void match(int expected, bool skip)
		{
		if (tok != expected && scan.keyword != expected)
			except("porttests syntax error on " << scan.value);
		next(skip);
		}

	void next(bool skip) 
		{
		while (true) {
			tok = scan.next();
			switch (tok)
				{
			case T_NEWLINE:
				if (!skip)
					return;
			case T_WHITE:
			case T_COMMENT:
				continue;
			default:
				return;
				}
			}
		}

	Scanner scan;
	int tok = 0;
	Ostream& os;
	};

bool PortTest::run_file(const char* filename, Ostream& os)
	{
	os << "<" << filename << ">" << endl;
	Parser parser(os, read_file(filename));
	return parser.run();
	}
