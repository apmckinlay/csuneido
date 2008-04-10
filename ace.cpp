/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2008 Suneido Software Corp. 
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

#include "cmdlineoptions.h"
#include "aceserver.h"
#include "errlog.h"
#include "ostreamstd.h"
#include "except.h"
#include "fatal.h"
#include <stdlib.h> // for exit
#include <exception>
#include "gc-7.0/include/gc.h"
#include "interp.h" // for proc

void builtins();

bool is_server = true;
bool is_client = false;
char* cmdline = "";
CmdLineOptions cmdlineoptions;
char* fiber_id = "";
char* session_id = "";

const int N_THREADS = 2; // for initial development

int main(int argc, char**argv)
	{
	GC_init();
	try
		{
		proc = new Proc;
		builtins(); // internal initialization
		aceserver(N_THREADS);
		}
	catch (const Except& x)
		{
		fatal(x.exception);
		}
	catch (const std::exception& e)
		{
		fatal(e.what());
		}
	catch (...)
		{
		fatal("unknown exception");
		}
	}

void alertout(char* s)
	{
	errlog("Alert: ", s);
	}

void ckinterrupt()
	{
	}

void fatal(const char* error, const char* extra)
	{
	cout << "fatal error: " << error << " " << extra << endl;
	errlog("fatal error:", error, extra);
	exit(-1);
	}
