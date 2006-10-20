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

// RichEdit support for save & load rtf
// based on VC++ reitp sample

#include "rich.h"
#include <richedit.h>
#include <vector>
using namespace std;

static DWORD CALLBACK addtostring(DWORD dwCookie, LPBYTE pbBuffer, LONG cb, LONG *pcb)
	{
	vector<char>* ps = (vector<char>*) dwCookie;
	ps->insert(ps->end(), (char*) pbBuffer, (char*) pbBuffer + cb);
	*pcb = cb;
	return NOERROR;
	}

gcstring RichEditGet(HWND hwndRE)
	{
	EDITSTREAM es;
	vector<char> s;

	es.dwCookie = (long) &s;
	es.dwError = 0;
	es.pfnCallback = addtostring;
	SendMessage(hwndRE, EM_STREAMOUT, (WPARAM) SF_RTF, (LPARAM) &es);
	return gcstring(s.size(), &s[0]); // no alloc constructor
	}

static DWORD CALLBACK takefromstring(DWORD dwCookie, LPBYTE pbBuffer, LONG cb, LONG *pcb)
	{
	gcstring* ps = (gcstring*) dwCookie;
	if (cb > ps->size())
		cb = ps->size();
	memcpy(pbBuffer, ps->buf(), cb);
	*ps = ps->substr(cb);
	*pcb = cb;
	return NOERROR;
	}

void RichEditPut(HWND hwndRE, gcstring s)
	{
	EDITSTREAM es;

	es.dwCookie = (long) &s;
	es.dwError = 0;
	es.pfnCallback = takefromstring;

	SendMessage(hwndRE, EM_STREAMIN, (WPARAM) SF_RTF, (LPARAM) &es);
	}
