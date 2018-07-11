// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostreamstd.h"
#include <stdio.h>

Ostream& OstreamStd::write(const void* s, int n)
	{
	fwrite(s, 1, n, stdout);
	return *this;
	}

void OstreamStd::flush()
	{
	fflush(stdout);
	}

OstreamStd cout;
