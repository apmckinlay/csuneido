/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2001 Suneido Software Corp.
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

#include "rebuildgui.h"
#include "win.h"
#include <commctrl.h>
#include "recover.h"
#include "resource.h"
#include "except.h"
#include "cmdlineoptions.h"

static BOOL CALLBACK check_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM);
static BOOL CALLBACK rebuild_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM);
static bool check();
static bool rebuild();

bool db_check_gui()
	{
	if (cmdlineoptions.unattended)
		return check();
	else
		{
		InitCommonControls();
		DialogBox((HINSTANCE) GetModuleHandle(NULL), "REBUILD", NULL, (DLGPROC) check_proc);
		return true;
		}
	}

static bool check()
	{
	DbRecover* dbr = DbRecover::check("suneido.db");
	switch (dbr->status())
		{
	case DBR_OK :
		{
		dbr->check_indexes();
		bool result = dbr->status() == DBR_OK;
		delete dbr;
		return result;
		}
	case DBR_ERROR :
	case DBR_UNRECOVERABLE :
		delete dbr;
		return false;
	default :
		unreachable();
		}
	}

bool db_rebuild_gui()
	{
	if (cmdlineoptions.unattended)
		return rebuild();
	else
		{
		InitCommonControls();
		BOOL result = DialogBox((HINSTANCE) GetModuleHandle(NULL), "REBUILD", NULL, 
			(DLGPROC) rebuild_proc);
		return result == TRUE;
		}
	}

static bool rebuild()
	{
	DbRecover* dbr = DbRecover::check("suneido.db");
	switch (dbr->status())
		{
	case DBR_OK :
		if (cmdlineoptions.check_start)
			return true;
		// else fall through to rebuild
	case DBR_ERROR :
		dbr->rebuild();
		delete dbr;
		return true;
	case DBR_UNRECOVERABLE :
		delete dbr;
		return false;
	default :
		unreachable();
		}
	}

static HWND hwnd_rebuild;
static bool rebuild_continue = true;

// pre: 0 <= pos < 100
static bool progress(int pos)
	{
	SendDlgItemMessage(hwnd_rebuild, IDC_PROGRESS1, PBM_SETPOS, pos, 0);
	// check for cancel
	MSG msg;
	while (PeekMessage(&msg, hwnd_rebuild, 0, 0, PM_REMOVE))
		{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		}
	return rebuild_continue;
	}

static BOOL CALLBACK check_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
	{
	static DbRecover* dbr;

	switch (uMsg)
		{
	case WM_INITDIALOG :
		SetWindowText(hDlg, "suneido -check");
		SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Checking data checksums ...");
		EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
		ShowWindow(hDlg, SW_SHOW);
		UpdateWindow(hDlg);
		hwnd_rebuild = hDlg; // for progress
		dbr = DbRecover::check("suneido.db", progress);
		if (dbr->status() == DBR_OK)
			{
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Checking indexes ...");
			dbr->check_indexes(progress);
			}
		EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);
		switch (dbr->status())
			{
		case DBR_OK :
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Database is okay, last commit:");
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_DATE), dbr->last_good_commit());
			break ;
		case DBR_ERROR :
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "DATABASE IS CORRUPTED, last good commit:");
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_DATE), dbr->last_good_commit());
			break ;
		case DBR_UNRECOVERABLE :
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "DATABASE IS NOT RECOVERABLE");
			break ;
		default :
			unreachable();
			}
		delete dbr;
		return TRUE;
	case WM_COMMAND :
		switch (LOWORD(wParam))
			{
		case IDOK :
		case IDCANCEL :
			EndDialog(hDlg, FALSE);
			delete dbr;
			return TRUE;
		default: 
			;
			}
		FALLTHROUGH
	default :
		return FALSE;
		}
	}

static BOOL CALLBACK rebuild_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
	{
	static DbRecover* dbr;

	switch (uMsg)
		{
	case WM_INITDIALOG :
		SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Checking data checksums ...");
		EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
		ShowWindow(hDlg, SW_SHOW);
		UpdateWindow(hDlg);
		hwnd_rebuild = hDlg; // for progress
		dbr = DbRecover::check("suneido.db", progress);
		if (dbr->status() == DBR_OK)
			{
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Checking indexes ...");
			dbr->check_indexes(progress);
			}
		switch (dbr->status())
			{
		case DBR_OK :
			if (cmdlineoptions.check_start)
				{
				EndDialog(hDlg, TRUE);
				return TRUE;
				}
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Database is okay, rebuild anyway?");
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_DATE), dbr->last_good_commit());
			EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
			break ;
		case DBR_ERROR :
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Rebuild up to last good commit:");
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_DATE), dbr->last_good_commit());
			EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
			break ;
		case DBR_UNRECOVERABLE :
			SetWindowText(GetDlgItem(hDlg, IDC_REBUILD_STATIC), "Database is not recoverable!");
			EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
			break ;
		default :
			unreachable();
			}
		progress(0);
		return TRUE;
	case WM_COMMAND :
		switch (LOWORD(wParam))
			{
		case IDOK :
			EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
			EndDialog(hDlg, dbr->rebuild(progress));
			delete dbr;
			return TRUE;
		case IDCANCEL :
			rebuild_continue = false;
			EndDialog(hDlg, FALSE);
			delete dbr;
			return TRUE;
		default: 
			;
			}
		FALLTHROUGH
	default :
		return FALSE;
		}
	}

