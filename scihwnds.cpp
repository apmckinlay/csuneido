// Copyright (c) 2019 Suneido Software Corp. All rights reserved
// Licensed under GPLv2
/**
 * Scintilla / Suneido Integration code:
 *	Related: vs2019scintilla -> ScintillaWin.cxx
 *	Purpose:
 *		Allows vs2019gc65 to keep track of Scintilla Hwnds:
 *		These Hwnds are stored in Windows memory.
 *		As such, we require a way of storing it in
 *		vs2019gc65 accessible memory for vs2019scintilla
 *		to function properly
 */
#include "win.h"
#include "htbl.h"

int SciHwnds_count = 0;
static Hset<LONG_PTR> hwndPtrs;

void SciHwnds_AddHwndPtr(LONG_PTR lngPtr) {
	hwndPtrs.insert(lngPtr);
	SciHwnds_count = hwndPtrs.size();
}

void SciHwnds_RmvHwndPtr(LONG_PTR lngPtr) {
	hwndPtrs.erase(lngPtr);
	SciHwnds_count = hwndPtrs.size();
}
