// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostreamstd.h"
#include <stdio.h>

class OstreamStd : public Ostream {
public:
	OstreamStd(FILE* f) : out(f) {
	}
	Ostream& write(const void* buf, int n);
	void flush();

private:
	FILE* out;
};

Ostream& OstreamStd::write(const void* s, int n) {
	fwrite(s, 1, n, out);
	return *this;
}

void OstreamStd::flush() {
	fflush(out);
}

static OstreamStd o(stdout);
static OstreamStd e(stderr);
Ostream& cout = o;
Ostream& cerr = e;
