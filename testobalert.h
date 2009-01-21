#ifndef TESTOBALERT_H
#define TESTOBALERT_H

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
#include "alert.h"
#include "ostreamstr.h"
#include "ostreamfile.h"

class TestObserverAlert : public TestObserver
	{
public:
	TestObserverAlert() : ntests(0)
		{
		OstreamFile os("test.log", "w"); // truncate
		}
	virtual void start_test(const char* group, const char* test)
		{
		OstreamFile os("test.log", "a");
		os << group << ' ' << test << endl;
		}
	virtual void end_test(const char* group, const char* test, char* error)
		{
		if (error)
			{
			OstreamFile os("test.log", "a");
			os << error << endl;
			errs << group << ' ' << test << " FAILED " << error << endl;
			}
		++ntests;
		}
	virtual void end_all(int nfailed)
		{
		OstreamStr os;
		os << ntests << " test" << (ntests > 1 ? "s" : "") << ' ';
		if (nfailed == 0)
			alert(os.str() << "ALL SUCCESSFUL");
		else
			alert(errs.str() << os.str() << nfailed << " FAILURE" << (nfailed == 1 ? "" : "S"));
		}
private:
	int ntests;
	OstreamStr errs;
	};

#endif
