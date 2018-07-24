#pragma once
// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// ensure unhook
struct WinHook {
	WinHook(int id, HOOKPROC proc, DWORD threadid) {
		hook = SetWindowsHookEx(id, proc, 0, threadid);
	}
	~WinHook() {
		UnhookWindowsHookEx(hook);
	}
	HHOOK hook;
};

class AutoWaitCursor {
public:
	AutoWaitCursor() {
		if (main_threadid == 0) {
			main_threadid = GetCurrentThreadId();
			DWORD threadid = 0;
			CreateThread(NULL, 1000, timer_thread, 0, 0, &threadid);
			// cast seems to be required by GCC 2.95.3
			static WinHook gmhook(
				WH_GETMESSAGE, (HOOKPROC) message_hook, main_threadid);
			hook = gmhook.hook;
		}

		ticks = 5; // start the timer
		prev_cursor = 0;
	}
	~AutoWaitCursor() {
		ticks = 0; // stop timer
		if (prev_cursor) {
			// restore cursor
			POINT pt;
			GetCursorPos(&pt);
			SetCursorPos(pt.x, pt.y);
		}
	}

private:
	static DWORD WINAPI timer_thread(LPVOID lpParameter) {
		HCURSOR wait_cursor = LoadCursor(NULL, IDC_WAIT);
		AttachThreadInput(GetCurrentThreadId(), main_threadid, true);
		for (;;) {
			Sleep(25); // milliseconds
			if (ticks > 0 && --ticks == 0)
				prev_cursor = SetCursor(wait_cursor);
		}
	}
	static LRESULT CALLBACK message_hook(
		int code, WPARAM wParam, LPARAM lParam) {
		ticks = 0; // stop timer
		return CallNextHookEx(hook, code, wParam, lParam);
	}
	static DWORD main_threadid;
	static HHOOK hook;
	static int volatile ticks;
	static HCURSOR volatile prev_cursor;
};
DWORD AutoWaitCursor::main_threadid = 0;
HHOOK AutoWaitCursor::hook = 0;
int volatile AutoWaitCursor::ticks = 0;
HCURSOR volatile AutoWaitCursor::prev_cursor = 0;
