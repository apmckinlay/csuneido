/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2004 Suneido Software Corp.
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
#include "testobstd.h"
#include "ostreamstr.h"
#include "malloc.h"
#include "except.h"
#include "ostreamstd.h"
#include <signal.h>
#include "cmdlineoptions.h"

bool is_server = false;
CmdLineOptions cmdlineoptions;

static void sigill(int signo)
	{
	except("illegal instruction");
	}

static void sigsegv(int signo)
	{
	except("memory fault");
	}

int main(int argc, char** argv)
	{
	signal(SIGILL, sigill);
	signal(SIGSEGV, sigsegv);

	TestObserverStd to;
	if (argc == 1)
		TestRegister::runall(to);
	for (--argc, ++argv; argc > 0; --argc, ++argv)
		if (-1 == TestRegister::runtest(*argv, to))
			cout << "can't find " << *argv << endl;
	return 0;
	}

Except::Except(char* x) : exception(x)
	{
	}

Ostream& operator<<(Ostream& os, const Except& x)
	{
	return os << x.exception;
	}

static OstreamStr os(200);

Ostream& osexcept()
	{
	return os;
	}

void except_()
	{
	Except x(os.str());
	os.clear();
	throw x;
	}
