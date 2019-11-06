// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

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
#include "fatal.h"
#include "sustring.h"
#include "unhandled.h"
#include "msgloop.h"
#include "port.h" // for fork_rebuild for start_check
#include "exceptimp.h"
#include "build.h"
#include "ostreamstr.h"
#include "ostreamfile.h"
#include "suservice.h"
#include <cstdio> // for remove
#include <process.h>

void connectToConsole(bool);

static_assert(sizeof(int) == 4, "int must be 32 bits / 4 bytes");
static_assert(sizeof(short) == 2, "short must be 16 bits / 2 bytes");
static_assert(sizeof(time_t) == 4, "time_t must be 32 bits / 4 bytes");

void builtins();

/**
 * Scintilla / Suneido Integration code:
 *	Related: vs2017scintilla -> Scintilla.h
 *	Purpose:
 *		Allows vs2017suneido to redirect Scintilla calls
 *		to vs2017scintilla
 */
extern "C" int Scintilla_RegisterClasses(void* hInstance);

static void init(HINSTANCE hInstance, LPSTR lpszCmdLine);
static void init2(HINSTANCE hInstance, LPSTR lpszCmdLine);
static void logPreviousErrors();
static void repl();

const char* cmdline = "";

bool is_server = false;
bool is_client = false;
CmdLineOptions cmdlineoptions;

void free_callbacks();

static VOID CALLBACK free_callbacks_timer(
	HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	free_callbacks();
}

int pascal WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int) {
	init(hInstance, lpszCmdLine);

	RegisterHotKey(nullptr, 0, MOD_CONTROL, VK_CANCEL);

	SetTimer(nullptr, 0, 50, free_callbacks_timer);
	message_loop(); // doesn't return

	return 0;
}

static void init(HINSTANCE hInstance, LPSTR lpszCmdLine) {
	try {
		init2(hInstance, lpszCmdLine);
	} catch (const Except& e) {
		fatal(e.str(), e.callstack());
	} catch (const std::exception& e) {
		fatal(e.what());
	}
#ifndef _MSC_VER
	// with Visual C++, this catches page faults ???
	catch (...) {
		fatal("unknown exception");
	}
#endif
}

void close_dbms() {
	Fibers::foreach_tls([](ThreadLocalStorage& tls) {
		delete tls.thedbms;
		tls.thedbms = nullptr;
	});
}

static void init2(HINSTANCE hInstance, LPSTR lpszCmdLine) {
	verify(memcmp("\xff", "\x01", 1) > 0); // ensure unsigned cmp

	Fibers::init();

	tls().proc = new Proc;
	builtins(); // internal initialization

	Scintilla_RegisterClasses(hInstance);
	cmdline = cmdlineoptions.parse(lpszCmdLine);

	if (!cmdlineoptions.no_exception_handling)
		unhandled();

	if (cmdlineoptions.install) {
		InstallService(cmdlineoptions.install);
		exit(EXIT_SUCCESS);
	} else if (cmdlineoptions.service) {
		CallServiceDispatcher(cmdlineoptions.service);
		exit(EXIT_SUCCESS);
	}

	connectToConsole(cmdlineoptions.action == REPL);

	switch (cmdlineoptions.action) {
	case NONE:
		break;
	case DUMP:
		dump(cmdlineoptions.argstr);
		exit(EXIT_SUCCESS);
	case LOAD:
		load(cmdlineoptions.argstr);
		exit(EXIT_SUCCESS);
	case CHECK: {
		bool ok = db_check_gui();
		exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	case REBUILD: {
		bool ok = db_rebuild_gui();
		exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	case DBDUMP:
		dbdump();
		exit(EXIT_SUCCESS);
	case TEST: {
		bool ok = run_tests(cmdlineoptions.argstr);
		exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	case BENCH: {
		OstreamStr os;
		run_benchmarks(os, cmdlineoptions.argstr);
		{
			OstreamFile log("bench.log", "w");
			log << os.gcstr();
		}
		alert(os.str());
		exit(EXIT_SUCCESS);
	}
	case SERVER:
		is_server = true;
		start_dbserver(cmdlineoptions.argstr);
		start_dbhttp();
		break;
	case CLIENT: {
		is_client = true;
		set_dbms_server_ip(cmdlineoptions.argstr);
		atexit(close_dbms);
		logPreviousErrors();
		break;
	}
	case REPL:
		repl();
		exit(EXIT_SUCCESS);
	case COMPACT:
		compact();
		exit(EXIT_SUCCESS);
	case UNINSTALL_SERVICE:
		UnInstallService();
		exit(EXIT_SUCCESS);
	case VERSION:
		alert("Built:  " << build
						 << "\n"
							"Copyright (C) 2000-2018 Suneido Software Corp.\n"
							"All rights reserved worldwide.\n"
							"Licensed under the GNU General Public License v2\n"
							"\n"
							"Boehm-Demers-Weiser garbage collector\n"
							"www.hpl.hp.com/personal/Hans_Boehm/gc");
		exit(EXIT_SUCCESS);
	default:
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

static void logPreviousErrors() {
	const int limit = 1000;
	char* filename = err_filename();
	if (FILE* f = fopen(filename, "r")) {
		char buf[1024] = "PREVIOUS: ";
		buf[sizeof buf - 1] = 0;
		int n = 0;
		for (; n < limit && fgets(buf + 10, sizeof buf - 10, f); ++n)
			dbms()->log(buf);
		fclose(f);
		if (buf[sizeof buf - 1] != 0)
			dbms()->log("ERROR: logPreviousErrors buffer overrun");
		if (n >= limit)
			dbms()->log("PREVIOUS: too many errors");
		remove(filename);
	}
}

// called by interp
void ckinterrupt() {
	MSG msg;

	if (HIWORD(GetQueueStatus(QS_HOTKEY))) {
		bool hotkey = false;
		while (PeekMessage(&msg, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE))
			hotkey = true;
		if (hotkey)
			except("interrupt");
	}
}

struct St {
	St(const char* s_, const char* t_, uint32_t to)
		: s(s_), t(t_), timeout(to) {
	}
	const char* s;
	const char* t;
	uint32_t timeout;
};

typedef int(__stdcall* MSGBOXAPI)(IN HWND hWnd, IN LPCSTR lpText,
	IN LPCSTR lpCaption, IN UINT uType, IN WORD wLanguageId,
	IN DWORD dwMilliseconds);

int __stdcall fallback(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption,
	IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds) {
	return MessageBox(hWnd, lpText, lpCaption, uType);
}

MSGBOXAPI getMessageBoxTimeout() {
	auto hUser32 = LoadLibrary("user32.dll");
	if (hUser32) {
		auto fn = (MSGBOXAPI) GetProcAddress(hUser32, "MessageBoxTimeoutA");
		FreeLibrary(hUser32);
		return fn;
	}
	// fallback to normal MessageBox, ignoring timeout
	return fallback;
}

DWORD WINAPI message_thread(void* p) {
	static auto MessageBoxTimeout = getMessageBoxTimeout();
	St* st = static_cast<St*>(p);
	MessageBoxTimeout(0, st->t, st->s,
		MB_OK | MB_TASKMODAL | MB_TOPMOST | MB_SETFOREGROUND, 0, st->timeout);
	return 0;
}

// timeout is used by fatal
void message(const char* s, const char* t, uint32_t timeout_ms = INFINITE) {
	St st(s, t, timeout_ms);
	HANDLE thread =
		CreateThread(nullptr, 0, message_thread, (void*) &st, 0, nullptr);
	if (thread)
		WaitForSingleObject(thread, INFINITE);
}

void handler(const Except& x) {
	if (tls().proc->in_handler) {
		message("Error in Handler", x.str());
		return;
	}
	tls().proc->in_handler = true;

	// TODO: use GetAncestor
	// determine top level window responsible
	HWND hwnd = 0; // msg.hwnd;
	//	while (HWND parent = GetParent(hwnd))
	//		hwnd = parent;

	try {
		call("Handler", Lisp<Value>((SuValue*) &x, (int) hwnd, x.calls()));
	} catch (const Except& e) {
		message("Error in Handler", e.str());
	}
	tls().proc->in_handler = false;
}

bool getSystemOption(const char* option, bool def_value) {
	if (SuObject* suneido = val_cast<SuObject*>(globals["Suneido"]))
		if (Value val = suneido->get(option))
			return (val == SuTrue) ? true
								   : (val == SuFalse) ? false : def_value;
	return def_value;
}

#include <io.h>
#include "func.h"
#include "ostreamstd.h"
#include "builtin.h"

BUILTIN(PrintStdout, "(s)") {
	const int nargs = 1;
	cout << ARG(0).gcstr();
	return Value();
}

static void repl() {
	globals["Suneido"].putdata("Print", globals["PrintStdout"]);
	cerr << "Built:  " << build << endl;
	cerr << "Note: use start/w suneido -repl" << endl;
	cerr << "Press Enter twice (i.e. blank line) to execute, q to quit" << endl;

	run("Init.Repl()");

	char buf[4096];
	char* lim = buf + sizeof buf;
	for (;;) {
		cerr << "~~~" << endl;
		char* dst = buf;
		while (fgets(dst, lim - dst, stdin)) {
			if (dst[0] == 'q' && dst[1] == '\n')
				return;
			if (dst[0] == '\n')
				break;
			dst += strlen(dst);
		}
		if (dst == buf)
			return; // eof
		try {
			if (Value x = run(buf)) {
				auto type = gcstring(x.type());
				auto ss = val_cast<SuString*>(x);
				if (ss && ss->gcstr().is_concat())
					type = " <concat>";
				else if (type == "String" || type == "Boolean" ||
					type == "Date")
					type = "";
				else if (val_cast<SuNumber*>(x))
					type = " <Dnum>";
				else if (x.is_int())
					type = " <int>";
				else
					type = " <" + type + ">";
				cout << x << type << endl;
			}
		} catch (Except& e) {
			cerr << e << endl;
			cerr << e.callstack() << endl;
		}
	}
}
