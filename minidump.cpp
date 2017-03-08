/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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

#include "win.h"
#include "dbghelp.h"

static void minidump2(PEXCEPTION_POINTERS xinfo)
	{
	HANDLE hFile = CreateFile("memory.dmp",
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	MINIDUMP_EXCEPTION_INFORMATION mxinfo;
	mxinfo.ThreadId = GetCurrentThreadId();
	mxinfo.ExceptionPointers = xinfo;
	mxinfo.ClientPointers = FALSE;
	MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		MiniDumpNormal,
		&mxinfo,
		NULL,
		NULL);
	CloseHandle(hFile);
	}

void minidump()
	{
	__try
		{
		RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
		}
	__except(minidump2(GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION)
		{
		}
	}
