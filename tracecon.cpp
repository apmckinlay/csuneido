/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2000 Suneido Software Corp. 
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

#include "trace.h"
#include "ostreamcon.h"
#include "ostreamfile.h"

int trace_level = 0;

class OstreamTrace : public Ostream
	{
public:
	OstreamTrace()
		{ }
	Ostream& write(const void* buf, int n)
		{
		if (trace_level & TRACE_CONSOLE)
			con().write(buf, n);
		if (trace_level & TRACE_LOGFILE)
			log().write(buf, n);
		return *this;
		}
	void flush()
		{
		if (trace_level & TRACE_CONSOLE)
			con().flush();
		if (trace_level & TRACE_LOGFILE)
			log().flush();
		}
	operator void*()
		{
		return this;
		}
	Ostream& con()
		{
		static OstreamCon con;
		return con;
		}
	Ostream& log()
		{
		static OstreamFile log("trace.log", "w");
		return log;
		}
	};


Ostream& tout()
	{
	static OstreamTrace t;
	return t;
	}
