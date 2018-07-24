// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "debug.h"
#include "ostreamstr.h"
#include "win.h"

static OstreamStr os(1000); // avoid allocation during debug output

Ostream& osdebug() {
	return os;
}

void debug_() {
	os << endl;
	OutputDebugString(os.str());
	os.clear();
}
