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
#include "dbcopy.h"
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
#include "splash.h"
#include "msgloop.h"
#include "port.h" // for fork_rebuild for start_check

#include "suservice.h"

void builtins();
bool iswindow();

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
	catch (const Except& x)
		{
		fatal(x.exception);
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
	verify(memcmp("\xff", "\x1", 1) > 0); // ensure unsigned cmp
	verify(sizeof (time_t) == 4); // required by Commit

	Fibers::init();

	tss_proc() = new Proc;
	builtins(); // internal initialization

	cmdline = cmdlineoptions.parse(lpszCmdLine);

	if (! cmdlineoptions.no_exception_handling)
		unhandled();
	
	if (cmdlineoptions.install)
		{
		// default to server
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
	case COPY :
		db_copy(cmdlineoptions.argstr);
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
		FILE* f = fopen("c:/suneido.err", "r");
		if (f)
			{
			char buf[1024] = "PREVIOUS: ";
			while (fgets(buf + 10, sizeof buf - 10, f))
				dbms()->log(buf);
			fclose(f);
			remove("c:/suneido.err");
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
		extern char* build_date;
		alert("Built:  " << build_date << "\n"
			""
			"Copyright (C) 2000-2007 Suneido Software Corp.\n"
			"All rights reserved worldwide.\n"
			"Licensed under the GNU General Public License.\n"
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
	HWND hdlg = 0;
	if (cmdlineoptions.splash)
		hdlg = splash(hInstance);
	
	if (cmdlineoptions.check_start)
		if (0 != fork_rebuild())
			fatal("Database corrupt, unable to start");

	if (run("Init()") == SuFalse)
		exit(EXIT_FAILURE);

	extern void master(char*);
	if (cmdlineoptions.replication == REP_MASTER)
		master(cmdlineoptions.slaveip);

	if (hdlg)
		DestroyWindow(hdlg);
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
	if (tss_proc()->in_handler)
		{
		message("Error in Error Handler", x.exception);
		return ;
		}
	tss_proc()->in_handler = true;

// TODO: use GetAncestor
	// determine top level window responsible
	HWND hwnd = 0; //msg.hwnd;
//	while (HWND parent = GetParent(hwnd))
//		hwnd = parent;
	
	try
		{
		SuObject* calls;
		if (Frame* f = x.fp)
			{
			calls = new SuObject;
			if (f->fn && f->fn->named.num == globals("Assert"))
				--f;
			for (; f > tss_proc()->frames; --f)
				{
				SuObject* call = new SuObject;
				SuObject* vars = new SuObject;
				if (f->fn)
					{
					call->put("fn", f->fn);
					int n;
					int i = f->fn->source(f->ip - f->fn->code - 1, &n);
					call->put("src_i", i);
					call->put("src_n", n);
					for (i = 0; i < f->fn->nlocals; ++i)
						if (f->local[i])
							vars->put(symbol(f->fn->locals[i]), f->local[i]);
					if (f->self)
						vars->put("this", f->self);
					}

				else
					{
					call->put("fn", f->prim);
					}

				call->put("locals", vars);
				calls->add(call);
				}
			}
		else
			calls = new SuObject;

		call("Handler", Lisp<Value>(new SuString(x.exception), (long) hwnd, calls));
		}
	catch (const Except& x)
		{
		message("Error in Debug", x.exception);
		}
	tss_proc()->in_handler = false;
	}
