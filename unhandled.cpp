// Copyright (c) 2005 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "unhandled.h"
#include "win.h"
#include "except.h"
#include "fibers.h" // for tls()
#include "itostr.h"
#include <cstdlib> // for exit

#define EXCEPT(x) \
	case EXCEPTION_##x: \
		except("win32 exception: " #x);
#define EXCEPTION(x) \
	case EXCEPTION_##x: \
		return (#x);

inline const char* exception_name(DWORD dw_code) {
	switch (dw_code) {
		EXCEPT(ACCESS_VIOLATION)
		EXCEPT(DATATYPE_MISALIGNMENT)
		EXCEPT(BREAKPOINT)
		EXCEPT(SINGLE_STEP)
		EXCEPT(ARRAY_BOUNDS_EXCEEDED)
		EXCEPT(FLT_DENORMAL_OPERAND)
		EXCEPT(FLT_DIVIDE_BY_ZERO)
		EXCEPT(FLT_INEXACT_RESULT)
		EXCEPT(FLT_INVALID_OPERATION)
		EXCEPT(FLT_OVERFLOW)
		EXCEPT(FLT_STACK_CHECK)
		EXCEPT(FLT_UNDERFLOW)
		EXCEPT(INT_DIVIDE_BY_ZERO)
		EXCEPT(INT_OVERFLOW)
		EXCEPT(PRIV_INSTRUCTION)
		EXCEPT(ILLEGAL_INSTRUCTION)
		EXCEPT(INVALID_HANDLE)
		EXCEPTION(IN_PAGE_ERROR)
		EXCEPTION(NONCONTINUABLE_EXCEPTION)
		EXCEPTION(STACK_OVERFLOW)
		EXCEPTION(INVALID_DISPOSITION)
		EXCEPTION(GUARD_PAGE)
	default:
		return "???";
	}
}

static HANDLE h_file = nullptr;

inline void write(const char* s) {
	static DWORD n;
	for (; *s; ++s) {
		if (*s == '\n')
			WriteFile(h_file, "\r", 1, &n, nullptr);
		WriteFile(h_file, s, 1, &n, nullptr);
	}
}

extern bool is_client;

[[noreturn]] inline void log_and_exit(
	const char* error, const char* extra = "") {
	static bool exiting = false;
	if (exiting)
		abort();
	exiting = true;

	// if client, write temporarily to suneido.err
	// to be picked up when client restarts
	h_file = CreateFile(is_client ? err_filename() : "error.log", GENERIC_WRITE,
		0, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (h_file != INVALID_HANDLE_VALUE) {
		SetFilePointer(h_file, 0, nullptr, FILE_END);
		if (*tls().fiber_id != 0) {
			write(tls().fiber_id);
			write(": ");
		}
		write("fatal error: ");
		write(error);
		if (*extra) {
			write(" ");
			write(extra);
		}
		write("\n");
		CloseHandle(h_file);
	}
	// would be nice to give user a message
	// but there doesn't seem to be a safe way to do so
	// if a stack overflow has occurred
	exit(-1);
}

// avoid using stack any more than absolutely necessary
// so exception_name and write are inline
static LONG WINAPI filter(EXCEPTION_POINTERS* p_info) {
	log_and_exit(exception_name(p_info->ExceptionRecord->ExceptionCode));
}

void fatal_log(const char* error, const char* extra) {
	log_and_exit(error, extra);
}

void unhandled() {
	SetUnhandledExceptionFilter(filter);
}

extern int su_port;

char* err_filename() {
	static char buf[512]; // avoid using stack space

	int n = GetTempPath(sizeof buf, buf);
	if (n == 0 || n > sizeof buf - 32)
		buf[0] = 0;
	strcat(buf, "suneido");
	itostr(su_port, buf + strlen(buf));
	strcat(buf, ".err");
	return buf;
}
