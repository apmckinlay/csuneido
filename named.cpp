// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "named.h"
#include "symbols.h"
#include "globals.h"

gcstring Named::name() const
	{
	gcstring s;
	if (parent && parent->name() != "")
		s = s + parent->name() + parent->separator;
	if (num)
		s += (num < 0x8000 ? globals(num) : symstr(num));
	else if (str)
		s += str;
	return s;
	}

gcstring Named::library() const
	{
	if (lib != "")
		return lib;
	else if (parent)
		return parent->library();
	else
		return "";
	}

#include "ostreamstr.h"

// for debugging
gcstring Named::info() const
	{
	OstreamStr os;
	os << "Named {";
	if (lib != "")
		os << " lib: " << lib;
	if (parent)
		os << ", parent: " << parent->info();
	os << ", sep: '" << separator << "'";
	if (num)
		os << ", num: " << (num < 0x8000 ? globals(num) : symstr(num));
	else if (str)
		os << ", str: " << str;
	os << " }";
	return os.gcstr();
	}
