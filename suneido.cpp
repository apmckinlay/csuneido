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

#include "std.h"
#include "rebuildgui.h"
#include "dbhttp.h"
#include "cmdlineoptions.h"
#include "win.h"
#include "value.h"
#include "interp.h"
#include "fibers.h"
#include "dump.h"
#include "load.h"
#include "testing.h"
#include "catstr.h"
#include "gcstring.h"
#include "alert.h"
#include "dbms.h"
#include "dbserver.h"
#include "sufunction.h"
#include "suobject.h"
#include "call.h"
#include "sunapp.h"
#include "dbcompact.h"
#include "recover.h"
#include "globals.h"
#include <ctype.h> // for isspace
#include <stdio.h> // for tmpnam and remove
#include "fatal.h"
#include "errlog.h"
#include "sustring.h"
#include "testobalert.h"
#include "prim.h"
#include "unhandled.h"
#include "msgloop.h"
#include "port.h" // for fork_rebuild for start_check
#include "exceptimp.h"
#include "build.h"

#include "suservice.h"

static_assert(sizeof(int64) == 8, "long long must be 64 bits / 8 bytes");
static_assert(sizeof(time_t) == 4, "time_t must be 32 bits / 4 bytes");

void builtins();

static void init(HINSTANCE hInstance, LPSTR lpszCmdLine);
static void init2(HINSTANCE hInstance, LPSTR lpszCmdLine);

BOOL CALLBACK splash(HWND, UINT uMsg, WPARAM, LPARAM)
	{
	return uMsg == WM_INITDIALOG ? TRUE : FALSE;
	}

char* cmdline = "";

bool is_server = false;
bool is_client = false;
CmdLineOptions cmdlineoptions;

int pascal WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int)
	{
	init(hInstance, lpszCmdLine);

	RegisterHotKey(0, 0, MOD_CONTROL, VK_CANCEL);

	message_loop(); // doesn't return

	return 0;
	}

static void init(HINSTANCE hInstance, LPSTR lpszCmdLine)
	{
	try
		{
		init2(hInstance, lpszCmdLine);
		}
	catch (const Except& e)
		{
		fatal(e.str());
		}
	catch (const std::exception& e)
		{
		fatal(e.what());
		}
#ifndef _MSC_VER
	// with Visual C++, this catches page faults ???
	catch (...)
		{
		fatal("unknown exception");
		}
#endif
	}

static void init2(HINSTANCE hInstance, LPSTR lpszCmdLine)
	{
	verify(memcmp("\xff", "\x01", 1) > 0); // ensure unsigned cmp

	Fibers::init();

	tls().proc = new Proc;
	builtins(); // internal initialization

	cmdline = cmdlineoptions.parse(lpszCmdLine);

	if (! cmdlineoptions.no_exception_handling)
		unhandled();

	if (cmdlineoptions.install)
		{
		InstallService(cmdlineoptions.install);
		exit(EXIT_SUCCESS);
		}
	else if (cmdlineoptions.service)
		{
		CallServiceDispatcher(cmdlineoptions.service);
		exit(EXIT_SUCCESS);
		}

	switch (cmdlineoptions.action)
		{
	case NONE :
		break ;
	case DUMP :
		dump(cmdlineoptions.argstr);
		exit(EXIT_SUCCESS);
	case LOAD :
		load(cmdlineoptions.argstr);
		exit(EXIT_SUCCESS);
	case CHECK :
		{
		bool ok = db_check_gui();
		exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	case REBUILD :
		{
		bool ok = db_rebuild_gui();
		exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	case DBDUMP :
		dbdump();
		exit(EXIT_SUCCESS);
	case TESTS :
		{
		TestObserverAlert to;
		TestRegister::runall(to);
		if (! *cmdline)
			exit(EXIT_SUCCESS);
		break ;
		}
	case TEST :
		{
		TestObserverAlert to;
		TestRegister::runtest(CATSTRA("test_", cmdlineoptions.argstr), to);
		exit(EXIT_SUCCESS);
		}
	case SERVER :
		is_server = true;
		start_dbserver(cmdlineoptions.argstr);
		start_dbhttp();
		break ;
	case CLIENT :
		{
		is_client = true;
		set_dbms_server_ip(cmdlineoptions.argstr);
		char* filename = err_filename();
		FILE* f = fopen(filename, "r");
		if (f)
			{
			char buf[1024] = "PREVIOUS: ";
			while (fgets(buf + 10, sizeof buf - 10, f))
				dbms()->log(buf);
			fclose(f);
			remove(filename);
			}
		break ;
		}
	case COMPACT :
		compact();
		exit(EXIT_SUCCESS);
	case UNINSTALL_SERVICE :
		UnInstallService();
		exit(EXIT_SUCCESS);
	case VERSION :
		alert("Built:  " << build_date << "\n"
			""
			"Copyright (C) 2000-2016 Suneido Software Corp.\n"
			"All rights reserved worldwide.\n"
			"Licensed under the GNU General Public License v2\n"
			"\n"
			"Boehm-Demers-Weiser garbage collector\n"
			"www.hpl.hp.com/personal/Hans_Boehm/gc"
			);
		exit(EXIT_SUCCESS);
	default :
		unreachable();
		}

#ifndef __GNUC__
	sunapp_register_classes();
#endif
	if (cmdlineoptions.check_start)
		if (0 != fork_rebuild())
			fatal("Database corrupt, rebuild failed, unable to start");

	if (run("Init()") == SuFalse)
		exit(EXIT_FAILURE);
	}

// called by interp
void ckinterrupt()
	{
	MSG msg;

	if (HIWORD(GetQueueStatus(QS_HOTKEY)))
		{
		bool hotkey = false;
		while (PeekMessage(&msg, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE))
			hotkey = true;
		if (hotkey)
			except("interrupt");
		}
	}

struct St
	{
	St(const char* s_, const char* t_) : s(s_), t(t_)
		{ }
	const char* s;
	const char* t;
	};

DWORD WINAPI message_thread(void* p)
	{
	St* st = (St*) p;
	MessageBox(0, st->t, st->s, MB_OK | MB_TASKMODAL | MB_TOPMOST | MB_SETFOREGROUND);
	return 0;
	}

void message(const char* s, const char* t)
	{
	St st(s, t);
	HANDLE thread = CreateThread(NULL, 0, message_thread, (void*) &st, 0, NULL);
	WaitForSingleObject(thread, INFINITE);
	}

void handler(const Except& x)
	{
	if (tls().proc->in_handler)
		{
		message("Error in Error Handler", x.str());
		return ;
		}
	tls().proc->in_handler = true;

// TODO: use GetAncestor
	// determine top level window responsible
	HWND hwnd = 0; //msg.hwnd;
//	while (HWND parent = GetParent(hwnd))
//		hwnd = parent;

	try
		{
		call("Handler", Lisp<Value>((SuValue*) &x, (long) hwnd, x.calls()));
		}
	catch (const Except& e)
		{
		message("Error in Debug", e.str());
		}
	tls().proc->in_handler = false;
	}

bool getSystemOption(const char* option, bool def_value)
	{
	if (SuObject* suneido = val_cast<SuObject*>(globals["Suneido"]))
		if (Value val = suneido->get(option))
			return (val == SuTrue) ? true : (val == SuFalse) ? false : def_value;
	return def_value;
	}
