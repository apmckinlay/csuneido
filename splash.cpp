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

#include "win.h"
#include "port.h"

// based on sample code from The Old New Thing by Raymond Chen

static HBRUSH CreatePatternBrushFromFile(LPCTSTR filename, HINSTANCE hInstance,
	int& width, int& height)
	{
	HBRUSH hbr = NULL;
	HBITMAP hbm = (HBITMAP) LoadImage(hInstance, filename,
		IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

	if (hbm)
		{
		BITMAP bm;
		GetObject(hbm, sizeof (bm), & bm);
		width = bm.bmWidth;
		height = bm.bmHeight;
		hbr = CreatePatternBrush(hbm);
		DeleteObject(hbm);
		}
	return hbr;
	}

static BOOL CreateClassFromFile(LPSTR filename, HINSTANCE hInstance,
	int& width, int& height)
	{
	HBRUSH hbr = CreatePatternBrushFromFile(filename, hInstance,
		width, height);
	if (! hbr)
		return FALSE;

	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_WAIT);
	wc.hbrBackground = hbr; // Do not delete the brush - the class owns it now
	wc.lpszMenuName = NULL;
	wc.lpszClassName = TEXT("splash");

	return RegisterClass(&wc);
	}

HWND splash(HINSTANCE hInstance)
	{
	char* filename = "splash.bmp";
	char buf[1024];
	get_exe_path(buf, sizeof buf);
	int n = strlen(buf) - 1; 
	while (n > 0 && buf[n] != '\\')
		--n;
	if (n + strlen(filename) + 2 >= sizeof buf)
		return NULL;
	strcpy(buf + n + 1, filename);

	int width, height;
	if (! CreateClassFromFile(buf, hInstance, width, height))
		return NULL;
	RECT wr;
	GetWindowRect(GetDesktopWindow(), &wr);
	int x = wr.left + (wr.right - wr.left - width) / 2;
	int y = wr.top + (wr.bottom - wr.top - height) / 2;
	HWND hdlg = CreateWindowEx(WS_EX_TOOLWINDOW, "splash", NULL, 
		WS_VISIBLE | WS_POPUP | SS_BITMAP,
		x, y, width, height, NULL, NULL, (HINSTANCE) hInstance, NULL);
	ShowWindow(hdlg, SW_SHOWNORMAL);
	UpdateWindow(hdlg);
	return hdlg;
	}

