#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2002 Suneido Software Corp.
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

enum { NONE = 0, DUMP, LOAD, SERVER, CLIENT, REPL, COMPACT,
	CHECK, REBUILD, DBDUMP, TEST, TESTS, BENCH, HELP, VERSION,
	UNINSTALL_SERVICE, AFTER_ACTIONS };

class CmdLineOptions
	{
public:
	CmdLineOptions()
		{ }
	const char* parse(const char* str);

	const char* s = nullptr;
	int action = NONE;
	const char* argstr = nullptr;
	int argint = 0;
	bool unattended = false;
	bool local_library = false;
	bool no_exception_handling = false;
	const char* install = nullptr;
	const char* service = nullptr;
	bool check_start = false;
	bool compact_exit = false;
	bool ignore_version = false;
	bool ignore_check = false;

private:
	int get_option();
	void set_action(int a);
	const char* get_word();
	void skip_white();
	static const char* strip_su(const char* file);
	};

extern CmdLineOptions cmdlineoptions;
