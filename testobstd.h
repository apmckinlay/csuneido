#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2004 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "testing.h"
#include "ostreamstd.h"

class TestObserverStd : public TestObserver
	{
public:
	TestObserverStd() : ntests(0)
		{ }

	void start_test(const char* test) override
		{ cout << "    " << test << " "; cout.flush(); }

	void end_test(const char* test, const char* error) override
		{
		if (error)
			cout << "FAILED " << error;
		cout << endl;
		++ntests;
		}

	void end_all(int nfailed) override
		{
		cout << ntests << " test" << (ntests > 1 ? "s" : "") << ' ';
		if (nfailed == 0)
			cout << "ALL SUCCESSFUL" << endl;
		else
			cout << nfailed << " FAILURE" << (nfailed == 1 ? "" : "S") << endl;
		}
private:
	int ntests;
	};
