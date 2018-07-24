// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include "dbghelp.h"

static void minidump2(PEXCEPTION_POINTERS xinfo) {
	HANDLE hFile = CreateFile("memory.dmp", GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	MINIDUMP_EXCEPTION_INFORMATION mxinfo;
	mxinfo.ThreadId = GetCurrentThreadId();
	mxinfo.ExceptionPointers = xinfo;
	mxinfo.ClientPointers = FALSE;
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
		MiniDumpNormal, &mxinfo, NULL, NULL);
	CloseHandle(hFile);
}

void minidump() {
	__try {
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
	} __except (
		minidump2(GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION) {
	}
}
