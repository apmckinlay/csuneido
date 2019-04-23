// Copyright (c) 2019 Suneido Software Corp. All rights reserved
// Licensed under GPLv2
/**
 * Scintilla / Suneido Integration code:
 *	Related: vs2017scintilla -> ScintillaWin.cxx
 *	Purpose:
 *		Allows vs2017gc65 to keep track of Scintilla Hwnds:
 *		These Hwnds are stored in Windows memory.
 *		As such, we require a way of storing it in
 *		vs2017gc65 accessible memory for vs2017scintilla
 *		to function properly
 */
#include "win.h"
#include "htbl.h"

int SciHwnds_count = 0;
static Hmap<int, LONG_PTR> hwnds;

void SciHwnds_AddHwndRef(HWND hwnd, LONG_PTR lngPtr) {
	hwnds.put(reinterpret_cast<int>(hwnd), lngPtr);
	SciHwnds_count = hwnds.size();
}

void SciHwnds_RmvHwndRef(HWND hwnd) {
	hwnds.erase(reinterpret_cast<int>(hwnd));
	SciHwnds_count = hwnds.size();
}
