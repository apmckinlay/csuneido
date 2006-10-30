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

void builtins();
bool iswindow();
static void message_loop();

BOOL CALLBACK splash(HWND, UINT uMsg, WPARAM, LPARAM)
	{
	return uMsg == WM_INITDIALOG ? TRUE : FALSE;
	}

char* cmdline = "";
HACCEL haccel = NULL;
HWND accel_hwnd = NULL; // to ensure haccel is meant for the active window

static MSG msg;
static bool exit_pending = false;
static int exit_status = 0;

Value su_exit()
	{
	const int nargs = 1;
	if (ARG(0) == SuTrue)
		exit(0);
	exit_status = ARG(0).integer();
	exit_pending = true;
	return Value();
	}
PRIM(su_exit, "Exit(status = 0)");

static BOOL CALLBACK destroy_func(HWND hwnd, LPARAM lParam)
	{
	DestroyWindow(hwnd);
	return true; // continue enumeration
	}

void do_exit()
	{
	// destroy windows
	EnumWindows(destroy_func, (LPARAM) NULL);

	PostQuitMessage(exit_status);
	}

bool is_server = false;
bool is_client = false;
CmdLineOptions cmdlineoptions;

int pascal WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int)
	{
	try
		{
		verify(memcmp("\xff", "\x1", 1) > 0); // ensure unsigned cmp
		verify(sizeof (time_t) == 4); // required by Commit

		Fibers::init();

		proc = new Proc;
		builtins(); // internal initialization

		cmdline = cmdlineoptions.parse(lpszCmdLine);

		if (! cmdlineoptions.no_exception_handling)
			unhandled();

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
			db_check_gui();
			exit(EXIT_SUCCESS);
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
			{
			char* tmp = tmpnam(NULL);
			if (*tmp == '\\')
				++tmp;
			db_copy(tmp);
			extern void close_db();
			close_db();
			remove("suneido.db.bak");
			if (0 != rename("suneido.db", "suneido.db.bak"))
				except("can't rename suneido.db to suneido.db.bak");
			if (0 != rename(tmp, "suneido.db"))
				except("can't rename temp file to suneido.db");
			exit(EXIT_SUCCESS);
			}
		case VERSION :
			extern char* build_date;
			alert("Built:  " << build_date << "\n"
				""
				"Copyright (C) 2000-2006 Suneido Software Corp.\n"
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
			{
			const int WIDTH = 321;
			const int HEIGHT = 380;
			RECT wr;
			GetWindowRect(GetDesktopWindow(), &wr);
			int x = wr.left + (wr.right - wr.left - WIDTH) / 2;
			int y = wr.top + (wr.bottom - wr.top - HEIGHT) / 2;
			hdlg = CreateWindowEx(WS_EX_TOOLWINDOW, "Static", "splashbmp", 
				WS_VISIBLE | WS_POPUP | SS_BITMAP,
				x, y, WIDTH, HEIGHT, NULL, NULL, (HINSTANCE) hInstance, NULL);
			ShowWindow(hdlg, SW_SHOWNORMAL);
			UpdateWindow(hdlg);
			}

		if (run("Init()") == SuFalse)
			exit(EXIT_FAILURE);

		if (cmdlineoptions.splash)
			DestroyWindow(hdlg);
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

	RegisterHotKey(0, 0, MOD_CONTROL, VK_CANCEL);

	message_loop();

	return 0;
	}

#include "awcursor.h"

static long lastinputtime = 0;

static void message_loop()
	{
	LPTSTR navigable = MAKEINTATOM(GlobalAddAtom("suneido_navigable"));

	for (;;)
		{
		// msg declared static above so it can be accessed by handler
		if (! PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{ 
			if (exit_pending)
				do_exit();
			else
				{
				Fibers::cleanup();
				Fibers::yield();
				}
			// at this point either message waiting or no runnable fibers
			GetMessage(&msg, 0, 0, 0);
			}

		AutoWaitCursor wc;

		if (msg.message == WM_QUIT)
			{
			int tn = dbms()->tranlist().size();
			if (tn > 0)
				if (is_server)
					{
					char buf[20];
					itoa(tn, buf, 10);
					errlog("uncompleted transactions:", buf);
					}
				else if (! is_client)
					alert(tn << " uncompleted transactions");
#ifndef __GNUC__
			sunapp_revoke_classes();
#endif
			exit(msg.wParam);
			}
		if ((WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST) ||
			(WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST))
			lastinputtime = msg.time;

		HWND activeWindow = GetActiveWindow();

		// NOTE: have to handle accelerators BEFORE IsDialogMessage
		if (haccel && activeWindow == accel_hwnd &&
			TranslateAccelerator(accel_hwnd, haccel, &msg))
			continue ;

		// let windows control navigation like dialogs
		HWND hwnd = msg.hwnd, parent;
		do
			parent = GetParent(hwnd);
			while (parent && ! GetProp(hwnd = parent, navigable));
		if (IsDialogMessage(hwnd, &msg))
			continue ;

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		extern void free_callbacks();
		free_callbacks();
		}
	// shouldn't get here
	}

Value su_lastinputtime()
	{
	return lastinputtime;
	}
PRIM(su_lastinputtime, "LastInputTime()");

void ckinterrupt()
	{
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
	DWORD threadId;
	HANDLE thread = CreateThread(NULL, 0, message_thread, (void*) &st, 0, &threadId);
	WaitForSingleObject(thread, INFINITE);
	}

void handler(const Except& x)
	{
	if (proc->in_handler)
		{
		message("Error in Error Handler", x.exception);
		return ;
		}
	proc->in_handler = true;

	// determine top level window responsible
	HWND hwnd = msg.hwnd;
	while (HWND parent = GetParent(hwnd))
		hwnd = parent;
	
	try
		{
		SuObject* calls;
		if (Frame* f = x.fp)
			{
			calls = new SuObject;
			if (f->fn && f->fn->named.num == globals("Assert"))
				--f;
			for (; f > proc->frames; --f)
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
	proc->in_handler = false;
	}
