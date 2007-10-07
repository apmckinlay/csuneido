#ifndef CMDLINEOPTIONS_H
#define CMDLINEOPTIONS_H

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

enum { NONE = 0, DUMP, LOAD, SERVER, CLIENT, COMPACT,
	CHECK, REBUILD, DBDUMP, COPY, TEST, TESTS, HELP, VERSION,
	UNINSTALL_SERVICE, AFTER_ACTIONS };

class CmdLineOptions
	{
public:
	CmdLineOptions() :
		action(NONE), argstr(0), argint(0), splash(true), unattended(false),
		local_library(false), no_exception_handling(false), install(0), service(0),
		check_start(false), compact_exit(false)
		{ }
	char* parse(char* str);
	
	char* s;
	int action;
	char* argstr;
	int argint;
	bool splash;
	bool unattended;
	bool local_library;
	bool no_exception_handling;
	char* install;
	char* service;
	bool check_start;
	bool compact_exit;

private:
	int get_option();
	void set_action(int a);
	char* get_word();
	char* get_string();
	void skip_white();
	char* strip_su(char* file);
	};

extern CmdLineOptions cmdlineoptions;

#endif
