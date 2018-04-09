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
#include "alert.h"
#include "dbms.h"
#include "dbserver.h"
#include "suobject.h"
#include "call.h"
#include "sunapp.h"
#include "dbcompact.h"
#include "recover.h"
#include "globals.h"
#include <stdio.h> // for remove
#include "fatal.h"
#include "sustring.h"
#include "testobalert.h"
#include "unhandled.h"
#include "msgloop.h"
#include "port.h" // for fork_rebuild for start_check
#include "exceptimp.h"
#include "build.h"

#include "suservice.h"
#include <fcntl.h>

static_assert(sizeof(int64) == 8, "long long must be 64 bits / 8 bytes");
static_assert(sizeof(time_t) == 4, "time_t must be 32 bits / 4 bytes");

void builtins();

static void init(HINSTANCE hInstance, LPSTR lpszCmdLine);
static void init2(HINSTANCE hInstance, LPSTR lpszCmdLine);
static void logPreviousErrors();
static void repl();

const char* cmdline = "";

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
		fatal(e.str(), e.callstack());
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

void close_dbms()
	{
	Fibers::foreach_tls([](ThreadLocalStorage& tls)
		{
		delete tls.thedbms;
		tls.thedbms = nullptr;
		});
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
	case TEST :
		{
		TestObserverAlert to;
		int nfailed = run_tests(to, cmdlineoptions.argstr);
		exit(nfailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	case BENCH :
		{
		OstreamStr os;
		run_benchmarks(os, cmdlineoptions.argstr);
		alert(os.str());
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
		atexit(close_dbms);
		logPreviousErrors();
		break ;
		}
	case REPL :
		repl();
		exit(EXIT_SUCCESS);
	case COMPACT :
		compact();
		exit(EXIT_SUCCESS);
	case UNINSTALL_SERVICE :
		UnInstallService();
		exit(EXIT_SUCCESS);
	case VERSION :
		alert("Built:  " << build << "\n"
			""
			"Copyright (C) 2000-2018 Suneido Software Corp.\n"
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

static void logPreviousErrors()
	{
	const int limit = 1000;
	char* filename = err_filename();
	if (FILE* f = fopen(filename, "r"))
		{
		char buf[1024] = "PREVIOUS: ";
		int n = 0;
		for (; n < limit && fgets(buf + 10, sizeof buf - 10, f); ++n)
			dbms()->log(buf);
		fclose(f);
		if (n >= limit)
			dbms()->log("PREVIOUS: too many errors");
		remove(filename);
		}
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
	St(const char* s_, const char* t_, ulong to) : s(s_), t(t_), timeout(to)
		{ }
	const char* s;
	const char* t;
	ulong timeout;
	};

typedef int(__stdcall *MSGBOXAPI)(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption,
	IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

int __stdcall fallback(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption,
	IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds)
	{
	return MessageBox(hWnd, lpText, lpCaption, uType);
	}

MSGBOXAPI getMessageBoxTimeout()
	{
	auto hUser32 = LoadLibrary("user32.dll");
	if (hUser32)
		{
		auto fn = (MSGBOXAPI)GetProcAddress(hUser32, "MessageBoxTimeoutA");
		FreeLibrary(hUser32);
		return fn;
		}
	// fallback to normal MessageBox, ignoring timeout
	return fallback;
	}

DWORD WINAPI message_thread(void* p)
	{
	static auto MessageBoxTimeout = getMessageBoxTimeout();
	St* st = static_cast<St*>(p);
	MessageBoxTimeout(0, st->t, st->s,
		MB_OK | MB_TASKMODAL | MB_TOPMOST | MB_SETFOREGROUND, 0, st->timeout);
	return 0;
	}

// timeout is used by fatal
void message(const char* s, const char* t, ulong timeout_ms = INFINITE)
	{
	St st(s, t, timeout_ms);
	HANDLE thread = CreateThread(nullptr, 0, message_thread, (void*) &st, 0, nullptr);
	if (thread)
		WaitForSingleObject(thread, INFINITE);
	}

void handler(const Except& x)
	{
	if (tls().proc->in_handler)
		{
		message("Error in Handler", x.str());
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
		message("Error in Handler", e.str());
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

#include <io.h>
#include "ostreamcon.h"
#include "func.h"

static ULONG_PTR GetParentProcessId()
	{
	ULONG_PTR pbi[6];
	ULONG ulSize = 0;
	LONG(WINAPI *NtQueryInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass,
		PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
	*(FARPROC *)&NtQueryInformationProcess =
		GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");
	if (NtQueryInformationProcess) {
		if (NtQueryInformationProcess(GetCurrentProcess(), 0,
			&pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi))
			return pbi[5];
		}
	return (ULONG_PTR)-1;
	}

static Value print()
	{
	const int nargs = 1;
	con() << ARG(0).gcstr();
	return Value();
	}

static void repl()
	{
	auto pid = GetParentProcessId();
	AttachConsole(pid); // need to use start/w
	static OstreamCon con;
	globals["Suneido"].putdata("Print",
		new Primitive("PrintCon(string)", print));
	HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
	con << "Built:  " << build << endl;
	run("Init.Repl()");
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(in), _O_TEXT);
	FILE* fin = _fdopen(fd, "r");
	verify(fin);
	char buf[1024];
	con << "> ";
	while (fgets(buf, sizeof buf, fin))
		{
		if (buf[0] == 'q' && buf[1] == '\n')
			break;
		try
			{
			Value x = run(buf);		
			if (x)
				con << x << endl;
			}
		catch (Except& e)
			{
			con << e << endl;
			con << e.callstack() << endl;
			}
		con << "> ";
		}
	}
