// Copyright (c) 2008 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "trace.h"
#include "ostreamstd.h"

int trace_level = 0;

Ostream& tout()
	{
	return cout;
	}
