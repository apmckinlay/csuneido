// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "getnum.h"
#include "except.h"
#include "gcstring.h"
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

// used by client-server text protocol

int getnum(char type, char*& s)
	{
	while (isspace(*s))
		++s;
	if (toupper(s[0]) != type ||
		! (isdigit(s[1]) || (s[1] == '-' && isdigit(s[2]))))
		return ERR;
	++s;
	int n = strtol(s, &s, 10);
	if (n < INT_MIN || INT_MAX < n)
		return ERR;
	while (isspace(*s))
		++s;
	return n;
	}

int ck_getnum(char type, char*& s)
	{
	int num = getnum(type, s);
	if (num == ERR)
		{
		gcstring t = gcstring::noalloc(s);
		if (t.has_suffix("\r\n"))
			t = t.substr(0, t.size() - 2);
		except("expecting: " << type << "#, got: " << t);
		}
	return num;
	}
