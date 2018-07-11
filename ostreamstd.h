#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

// an output stream to stdout
class OstreamStd : public Ostream
	{
public:
	Ostream& write(const void* buf, int n);
	void flush();
	};

extern OstreamStd cout;

#define SHOW(stuff) cout << #stuff << " => " << (stuff) << endl
