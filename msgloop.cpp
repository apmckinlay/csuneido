/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2007 Suneido Software Corp.
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

#include "msgloop.h"
#include <stdlib.h> // for exit
#include "awcursor.h"
#include "fibers.h"
#include "sunapp.h"
#include "except.h"
#include "dbms.h"

void free_callbacks();

static void shutdown(int);

void message_loop(HWND hdlg)
	{
	MSG msg;

	for (;;)
		{
		if (hdlg && GetWindowLong(hdlg, GWL_USERDATA) == 1)
			return ;

		while (! PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
			SleepEx(0, true); // run completion routines (may unblock fibers)
			if (! Fibers::yield())
				// no runnable fibers so wait for event for up to 20 ms
				// need to wait so we don't run continuously
				// note: timeout accuracy is probably only ~16ms (Windows tick)
				// MUST be Ex version to be alertable for overlapping socket io
				MsgWaitForMultipleObjectsEx(0, nullptr, 20, QS_ALLINPUT, MWMO_ALERTABLE);
			}

		AutoWaitCursor wc;

		if (msg.message == WM_QUIT)
			{
			if (hdlg)
				{
				PostQuitMessage(msg.wParam);
				return ;
				}
			else
				shutdown(msg.wParam);
			}

		HWND window = GetAncestor(msg.hwnd, GA_ROOT);

		if (HACCEL haccel = (HACCEL) GetWindowLong(window, GWL_USERDATA))
			if (TranslateAccelerator(window, haccel, &msg))
				continue ;

		if (IsDialogMessage(window, &msg))
			continue ;

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		free_callbacks();
		}
	unreachable();
	}

static BOOL CALLBACK destroy_func(HWND hwnd, LPARAM lParam)
	{
	DestroyWindow(hwnd);
	return true; // continue enumeration
	}

static void destroy_windows()
	{
	EnumWindows(destroy_func, (LPARAM) NULL);
	}

#include "cmdlineoptions.h" // for compact_exit
#include "dbcompact.h" // for compact

static void shutdown(int status)
	{
	destroy_windows(); // so they can do save etc.
#ifndef __GNUC__
	sunapp_revoke_classes();
#endif
	Fibers::foreach_tls([](ThreadLocalStorage& tls)
		{
		delete tls.thedbms;
		tls.thedbms = nullptr;
		});
	if (cmdlineoptions.compact_exit)
		compact();
	exit(status);
	}
