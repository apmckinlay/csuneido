#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

void noargs(
	short nargs, short nargnames, short* argnames, int each, const char* usage);

#define NOARGS(usage) \
	if (nargs != 0) \
	noargs(nargs, nargnames, argnames, each, usage)
