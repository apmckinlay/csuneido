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

// RichEdit support for inserting OLE objects
// minimal version - NO support for in-place activation
// based on VC++ reitp sample

#include "win.h"
#include <windowsx.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>

#ifdef __MINGW32__
//const CLSID IID_IRichEditOle = { 0x00020D00, 0x00, 0x00, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
#endif
const CLSID MyIID_IRichEditOleCallback = { 0x00020D03, 0x00, 0x00, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

typedef struct
	{
	IRichEditOleCallbackVtbl* lpVtbl;
	ULONG cRef;
	LPSTORAGE pstg;
	ULONG cItem;
	WNDPROC wndproc;
	} ITPCALL;

STDMETHODIMP ITPCALL_QueryInterface(LPUNKNOWN punk, REFIID riid,
								    LPUNKNOWN * ppvObj);
STDMETHODIMP_(ULONG) ITPCALL_AddRef(LPUNKNOWN punk);
STDMETHODIMP_(ULONG) ITPCALL_Release(LPUNKNOWN punk);
STDMETHODIMP ITPCALL_GetNewStorage(ITPCALL * pitpcall, LPSTORAGE FAR * ppstg);
STDMETHODIMP ITPCALL_GetInPlaceContext(ITPCALL * pitpcall, 
									   LPOLEINPLACEFRAME FAR * ppipframe,
									   LPOLEINPLACEUIWINDOW FAR* ppipuiDoc,
									   LPOLEINPLACEFRAMEINFO pipfinfo);
STDMETHODIMP ITPCALL_ShowContainerUI(ITPCALL * pitpcall, BOOL fShow);
STDMETHODIMP ITPCALL_QueryInsertObject(ITPCALL * pitpcall, LPCLSID pclsid,
				LPSTORAGE pstg, LONG cp);
STDMETHODIMP ITPCALL_DeleteObject(ITPCALL * pitpcall, LPOLEOBJECT poleobj);
STDMETHODIMP ITPCALL_QueryAcceptData(ITPCALL * pitpcall, LPDATAOBJECT pdataobj,
				CLIPFORMAT *pcfFormat, DWORD reco, BOOL fReally,
				HGLOBAL hMetaPict);
STDMETHODIMP ITPCALL_ContextSensitiveHelp(ITPCALL * pitpcall, BOOL fEnterMode);
STDMETHODIMP ITPCALL_GetClipboardData(ITPCALL *pitpcall, CHARRANGE *pchrg,
				DWORD reco, LPDATAOBJECT *ppdataobj);
STDMETHODIMP ITPCALL_GetDragDropEffect(ITPCALL *pitpcall, BOOL fDrag,
				DWORD grfKeyState, LPDWORD pdwEffect);
STDMETHODIMP ITPCALL_GetContextMenu(ITPCALL *pitpcall, WORD seltype,
				LPOLEOBJECT poleobj, CHARRANGE * pchrg, HMENU * phmenu);

IRichEditOleCallbackVtbl ITPCALL_Vtbl =
	{
	(LPVOID) ITPCALL_QueryInterface,
	(LPVOID) ITPCALL_AddRef,
	(LPVOID) ITPCALL_Release,
	(LPVOID) ITPCALL_GetNewStorage,
	(LPVOID) ITPCALL_GetInPlaceContext,
	(LPVOID) ITPCALL_ShowContainerUI,
	(LPVOID) ITPCALL_QueryInsertObject,
	(LPVOID) ITPCALL_DeleteObject,
	(LPVOID) ITPCALL_QueryAcceptData,
	(LPVOID) ITPCALL_ContextSensitiveHelp,
	(LPVOID) ITPCALL_GetClipboardData,
	(LPVOID) ITPCALL_GetDragDropEffect,
	(LPVOID) ITPCALL_GetContextMenu
	};

STDMETHODIMP ITPCALL_QueryInterface(LPUNKNOWN punk, REFIID riid, 
	LPUNKNOWN * ppvObj)
	{
	if(IsEqualIID(riid, &IID_IUnknown))
		{ ITPCALL_AddRef(*ppvObj = punk); return S_OK; }
	else if(IsEqualIID(riid, &MyIID_IRichEditOleCallback))
		{ ITPCALL_AddRef(*ppvObj = punk); return S_OK; }
	else
		{ *ppvObj = NULL; return E_NOINTERFACE; }
	}

STDMETHODIMP_(ULONG) ITPCALL_AddRef(LPUNKNOWN punk)
	{
	return ++((ITPCALL* ) punk)->cRef;
	}

STDMETHODIMP_(ULONG) ITPCALL_Release(LPUNKNOWN punk)
	{
	ITPCALL* pitpcall = (ITPCALL* ) punk;

	if (--pitpcall->cRef == 0)
		{
		pitpcall->pstg->lpVtbl->Release(pitpcall->pstg);
		(void) GlobalFreePtr(pitpcall);
		}

	return pitpcall->cRef;
	}

STDMETHODIMP ITPCALL_GetNewStorage(ITPCALL * pitpcall, LPSTORAGE FAR * ppstg)
	{
	WCHAR szItemW[256];

	wsprintfW(szItemW, L"REOBJ%ld", ++pitpcall->cItem);
	if (NOERROR != pitpcall->pstg->lpVtbl->CreateStorage(pitpcall->pstg, szItemW,
            STGM_READWRITE | STGM_TRANSACTED | STGM_SHARE_EXCLUSIVE,
            0, 0, ppstg))
			return E_FAIL;
	return S_OK;
	}

STDMETHODIMP ITPCALL_GetInPlaceContext(ITPCALL * pitpcall, 
	LPOLEINPLACEFRAME FAR * ppipframe,
	LPOLEINPLACEUIWINDOW FAR* ppipuiDoc,
	LPOLEINPLACEFRAMEINFO pipfinfo)
	{
	return E_NOTIMPL;
	}

STDMETHODIMP ITPCALL_ShowContainerUI(ITPCALL * pitpcall, BOOL fShow)
	{
	return E_NOTIMPL;
	}

STDMETHODIMP ITPCALL_QueryInsertObject(ITPCALL * pitpcall, LPCLSID pclsid,
	LPSTORAGE pstg, LONG cp)
	{
	return NOERROR;
	}

STDMETHODIMP ITPCALL_DeleteObject(ITPCALL * pitpcall, LPOLEOBJECT poleobj)
	{
	return NOERROR;
	}

STDMETHODIMP ITPCALL_QueryAcceptData(ITPCALL * pitpcall, LPDATAOBJECT pdataobj,
	CLIPFORMAT *pcfFormat, DWORD reco, BOOL fReally,
	HGLOBAL hMetaPict)
	{
	return NOERROR;
	}

STDMETHODIMP ITPCALL_ContextSensitiveHelp(ITPCALL * pitpcall, BOOL fEnterMode)
	{
	return E_NOTIMPL;
	}

STDMETHODIMP ITPCALL_GetClipboardData(ITPCALL *pitpcall, CHARRANGE *pchrg,
	DWORD reco, LPDATAOBJECT *ppdataobj)
	{
	return E_NOTIMPL;
	}

STDMETHODIMP ITPCALL_GetDragDropEffect(ITPCALL *pitpcall, BOOL fDrag,
	DWORD grfKeyState, LPDWORD pdwEffect)
	{
	return E_NOTIMPL;
	}

STDMETHODIMP ITPCALL_GetContextMenu(ITPCALL *pitpcall, WORD seltype,
	LPOLEOBJECT poleobj, CHARRANGE * pchrg, HMENU * phmenu)
	{
	return E_NOTIMPL;
	}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
	ITPCALL* pitpcall = (ITPCALL*) GetWindowLong(hwnd, GWL_USERDATA);
	WNDPROC wndproc = pitpcall->wndproc;
	if (msg == WM_NCDESTROY && pitpcall)
		{
		SetWindowLong(hwnd, GWL_USERDATA, 0);
		SetWindowLong(hwnd, GWL_WNDPROC, (long) wndproc);
		ITPCALL_Release((LPUNKNOWN) pitpcall);
		}
	return CallWindowProc(wndproc, hwnd, msg, wparam, lparam);
	}

// WARNING: uses GWL_USERDATA, application must not alter!!!
void RichEditOle(HWND hwndRE)
	{
	ITPCALL* pitpcall;
	if (! (pitpcall = (ITPCALL *) GlobalAllocPtr(GHND, sizeof(ITPCALL))))
		return ; // failed
	pitpcall->lpVtbl = &ITPCALL_Vtbl;
	pitpcall->cRef = 1;					// Start with one reference
	pitpcall->cItem = 0;

	OleInitialize(NULL);

	if (NOERROR != StgCreateDocfile(NULL,
         STGM_SHARE_EXCLUSIVE | STGM_READWRITE | STGM_TRANSACTED | STGM_DELETEONRELEASE,
         0, &pitpcall->pstg))
		 return ; // failed

	SetWindowLong(hwndRE, GWL_USERDATA, (LONG) pitpcall);
	pitpcall->wndproc = (WNDPROC) GetWindowLong(hwndRE, GWL_WNDPROC);
	SetWindowLong(hwndRE, GWL_WNDPROC, (long) wndproc);
	SendMessage(hwndRE, EM_SETOLECALLBACK, 0, (LPARAM) pitpcall);
	}
