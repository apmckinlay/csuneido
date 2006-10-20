/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2005 Suneido Software Corp. 
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

#include "unhandled.h"
#include "win.h"
#include "except.h"
#include <stdlib.h> // for exit

#define EXCEPT(x) case EXCEPTION_##x: except("win32 exception: " #x);
#define EXCEPTION(x) case EXCEPTION_##x: return (#x);

inline const char* exception_name(DWORD dw_code)
	{
	switch (dw_code)
		{
	EXCEPT( ACCESS_VIOLATION )
	EXCEPT( DATATYPE_MISALIGNMENT )
	EXCEPT( BREAKPOINT )
	EXCEPT( SINGLE_STEP )
	EXCEPT( ARRAY_BOUNDS_EXCEEDED )
	EXCEPT( FLT_DENORMAL_OPERAND )
	EXCEPT( FLT_DIVIDE_BY_ZERO )
	EXCEPT( FLT_INEXACT_RESULT )
	EXCEPT( FLT_INVALID_OPERATION )
	EXCEPT( FLT_OVERFLOW )
	EXCEPT( FLT_STACK_CHECK )
	EXCEPT( FLT_UNDERFLOW )
	EXCEPT( INT_DIVIDE_BY_ZERO )
	EXCEPT( INT_OVERFLOW )
	EXCEPT( PRIV_INSTRUCTION )
	EXCEPT( ILLEGAL_INSTRUCTION )
	EXCEPT( INVALID_HANDLE )
	EXCEPTION( IN_PAGE_ERROR )
	EXCEPTION( NONCONTINUABLE_EXCEPTION )
	EXCEPTION( STACK_OVERFLOW )
	EXCEPTION( INVALID_DISPOSITION )
	EXCEPTION( GUARD_PAGE )
	default: return "???";
		}
	}
	
static HANDLE h_file = 0;

inline void write(const char* s)
	{
	static DWORD n;
	WriteFile(h_file, s, strlen(s), &n, 0);
	}

inline void log_and_exit(const char* error, const char* extra = "")
	{
	static bool exiting = false;
	if (exiting)
		abort();
	exiting = true;

	// if client, write temporarily to c:/suneido.err
	// to be picked up when client restarts
	extern bool is_client;
	h_file = CreateFile(
		is_client ? "c:/suneido.err" : "error.log",
		GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
	if (h_file != INVALID_HANDLE_VALUE)
		{
		SetFilePointer(h_file, 0, 0, FILE_END);
		extern char* session_id;
		write(session_id);
		if (*session_id != 0)
			write(": ");
		write("fatal error: ");
		write(error);
		if (*extra)
			{
			write(" ");
			write(extra);
			}
		write("\r\n");
		CloseHandle(h_file);
		}
	// would be nice to give user a message
	// but there doesn't seem to ba a safe way to do so
	// if a stack overflow has occurred
	exit(-1);
	}

// avoid using stack any more than absolutely necessary
// so exception_name and write are inline
static LONG WINAPI filter(EXCEPTION_POINTERS* p_info)
	{
	log_and_exit(exception_name(p_info->ExceptionRecord->ExceptionCode));
	return 0; // not used
	}

void fatal_log(const char* error, const char* extra)
	{
	log_and_exit(error, extra);
	}

void unhandled()
	{
	SetUnhandledExceptionFilter(filter);
	}
