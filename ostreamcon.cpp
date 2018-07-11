// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostreamcon.h"
#include "win.h"

class OstreamConImp
	{
public:
	OstreamConImp()
		{
		AllocConsole();
		con = GetStdHandle(STD_OUTPUT_HANDLE);
		}
	void close()
		{ FreeConsole(); }
	void add(const void* s, int n)
		{
		DWORD nw;
		WriteFile(con, s, n, &nw, NULL);
		}
	explicit operator bool() const
		{ return con; }
private:
	HANDLE con;
	};

OstreamCon::OstreamCon() : imp(new OstreamConImp)
	{ }

OstreamCon::~OstreamCon()
	{ imp->close(); }

Ostream& OstreamCon::write(const void* s, int n)
	{
	imp->add(s, n);
	return *this;
	}

OstreamCon::operator bool() const
	{
	return bool(*imp);
	}

Ostream& con()
	{
	static OstreamCon con;
	return con;
	}
