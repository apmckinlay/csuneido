// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suwinres.h"
#include "except.h"
#include "value.h"
#include "win.h"
#include "noargs.h"

// The purpose is to track whether handles and gdiobj are closed

SuWinRes::SuWinRes(void* handle) : h(handle) {
}

Value SuWinRes::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	static Value Close("Close");

	if (member == Close) {
		NOARGS("handle.Close()");
		return close() ? SuTrue : SuFalse;
	} else
		return SuValue::call(self, member, nargs, nargnames, argnames, each);
}

//===================================================================

int handle_count = 0;

SuHandle::SuHandle(void* handle) : SuWinRes(handle) {
	++handle_count;
}

void SuHandle::out(Ostream& os) const {
	os << "handle " << h;
}

bool SuHandle::close() {
	--handle_count;
	bool ok = h && CloseHandle((HANDLE) h);
	h = 0;
	return ok;
}

//===================================================================

int gdiobj_count = 0;

SuGdiObj::SuGdiObj(void* handle) : SuWinRes(handle) {
	++gdiobj_count;
}

void SuGdiObj::out(Ostream& os) const {
	os << "gdiobj " << h;
}

int SuGdiObj::integer() const {
	return (int) h;
}

bool SuGdiObj::close() {
	--gdiobj_count;
	bool ok = h && DeleteObject((HGDIOBJ) h);
	h = 0;
	return ok;
}
