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

#include "cmdlineoptions.h"
#include <stdlib.h>
#include <ctype.h>
#include "alert.h"
#include "except.h"
#include "gcstring.h"

enum { PORT = AFTER_ACTIONS, NOSPLASH, UNATTENDED, LOCAL_LIBRARY,
	NO_EXCEPTION_HANDLING, NO_GARBAGE_COLLECTION };

char* CmdLineOptions::parse(char* str)
	{
	s = str;
	while (int opt = get_option())
		{
		switch (opt)
			{
		// actions
		case DUMP :
		case LOAD :
			if (0 != (argstr = get_word()))
				strip_su(argstr);
			else
				argstr = "";
			break ;
		case COPY :
			if (! (argstr = get_word()))
				argstr = "suneido.db.copy";
			break ;
		case TEST :
			if (*s == '_')
				++s;
			if (! (argstr = get_word()))
				action = TESTS;
			break ;
		case SERVER :
			if (*s == '=')
				{
				++s;
				argstr = get_word();
				}
			else
				argstr = "Suneido";
			break ;
		case CLIENT :
			if (! (argstr = get_word()))
				argstr = "127.0.0.1";
			break ;
		case TESTS :
		case CHECK :
		case REBUILD :
		case DBDUMP :
		case COMPACT :
		case VERSION :
			break ;
		// options
		case PORT :
			extern int su_port;
			su_port = strtol(s, &s, 10);
			break ;
		case NOSPLASH :
			splash = false;
			break ;
		case UNATTENDED :
			unattended = true;
			break ;
		case LOCAL_LIBRARY :
			local_library = true;
			break ;
		case NO_EXCEPTION_HANDLING :
			no_exception_handling = true;
			break ;
		case NO_GARBAGE_COLLECTION :
			break ;
		case HELP :
			alert("options:\n"
				"	-check\n"
				"	-r[ebuild]\n"
				"	-copy [filename]\n"
				"	-compact\n"
				"	-d[ump] [tablename]\n"
				"	-l[oad] [tablename]\n"
				"	-t[ests]\n"
				"	-test name\n"
				"	-s[erver][=name]\n"
				"	-c[lient] ipaddress\n"
				"	-p[ort] #\n"
				"	-n[osplash]\n"
				"	-u[nattended]\n"
				"	-v[ersion]\n"
				"	-l[ocal]l[ibrary]\n"
				"	-e[xception]h[andling]\n"
				"	-g[arbage]c[ollection]\n"
				);
			exit(EXIT_SUCCESS);
		default :
			unreachable();
			}
		}
	return s;
	}

static struct { char* str; int num; } options[] = { 
	{ "-dbdump", DBDUMP },
	{ "-dump", DUMP }, { "-d", DUMP },
	{ "-locallibrary", LOCAL_LIBRARY }, { "-ll", LOCAL_LIBRARY },
	{ "-load", LOAD }, { "-l", LOAD },
	{ "-server", SERVER }, { "-s", SERVER },
	{ "-copy", COPY },
	{ "-compact", COMPACT },
	{ "-check", CHECK },
	{ "-client", CLIENT }, { "-c", CLIENT },
	{ "-eh", NO_EXCEPTION_HANDLING }, { "-exceptionhandling", NO_EXCEPTION_HANDLING },
	{ "-gc", NO_GARBAGE_COLLECTION }, { "-garbagecollection", NO_GARBAGE_COLLECTION },
	{ "-rebuild", REBUILD }, { "-r", REBUILD },
	{ "-tests", TESTS }, 
	{ "-test", TEST },
	{ "-t", TESTS },
	{ "-port", PORT }, { "-p", PORT },
	{ "-nosplash", NOSPLASH }, { "-n", NOSPLASH }, 
	{ "-help", HELP }, { "-?", HELP }, { "-h", HELP },
	{ "-unattended", UNATTENDED }, { "-u", UNATTENDED },
	{ "-version", VERSION }, { "-v", VERSION }
	};
const int noptions = sizeof options / sizeof options[0];

int CmdLineOptions::get_option()
	{
	skip_white();
	for (int i = 0; i < noptions; ++i)
		if (has_prefix(s, options[i].str))
			{
			s += strlen(options[i].str);
			if (options[i].num < AFTER_ACTIONS)
				set_action(options[i].num);
			return options[i].num;
			}
	return 0;
	}

void CmdLineOptions::skip_white()
	{
	while (*s && isspace(*s))
		++s;
	}

void CmdLineOptions::set_action(int a)
	{
	if (action)
		except("Please specify only one of: -dump, -load, -server, -client, -check, -rebuild, -copy, -test");
	action = a;
	}

char* CmdLineOptions::get_word()
	{
	skip_white();
	if (*s == 0 || *s == '-' || *s == '+')
		return 0;
	char* word = s;
	while (*s && ! isspace(*s))
		++s;
	if (*s) // && isspace(*s)
		*s++ = 0;
	return *word ? word : 0;
	}

char* CmdLineOptions::strip_su(char* file)
	{
	if (has_suffix(file, ".su"))
		file[strlen(file) - 3] = 0;
	return file;
	}

