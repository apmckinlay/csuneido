// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// RichEdit support for save & load rtf
// based on VC++ reitp sample

#include "rich.h"
#include <richedit.h>
#include <vector>
using namespace std;

static DWORD CALLBACK addtostring(
	DWORD dwCookie, LPBYTE pbBuffer, LONG cb, LONG* pcb) {
	vector<char>* ps = (vector<char>*) dwCookie;
	ps->insert(ps->end(), (char*) pbBuffer, (char*) pbBuffer + cb);
	*pcb = cb;
	return NOERROR;
}

gcstring RichEditGet(HWND hwndRE) {
	EDITSTREAM es;
	vector<char> s;

	es.dwCookie = (long) &s;
	es.dwError = 0;
	es.pfnCallback = addtostring;
	SendMessage(hwndRE, EM_STREAMOUT, (WPARAM) SF_RTF, (LPARAM) &es);
	return gcstring::noalloc(&s[0], s.size());
}

static DWORD CALLBACK takefromstring(
	DWORD dwCookie, LPBYTE pbBuffer, LONG cb, LONG* pcb) {
	gcstring* ps = (gcstring*) dwCookie;
	if (cb > ps->size())
		cb = ps->size();
	memcpy(pbBuffer, ps->ptr(), cb);
	*ps = ps->substr(cb);
	*pcb = cb;
	return NOERROR;
}

void RichEditPut(HWND hwndRE, gcstring s) {
	EDITSTREAM es;

	es.dwCookie = (long) &s;
	es.dwError = 0;
	es.pfnCallback = takefromstring;

	SendMessage(hwndRE, EM_STREAMIN, (WPARAM) SF_RTF, (LPARAM) &es);
}
