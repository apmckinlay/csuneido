#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include "gcstring.h"

extern "C" void RichEditOle(long);

gcstring RichEditGet(HWND hwndRE);
void RichEditPut(HWND hwndRE, gcstring s);
