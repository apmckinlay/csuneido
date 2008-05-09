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

#include "except.h"
#include "interp.h"
#include "ostreamstr.h"
#include "gcstring.h"

Except::Except(char* x) : exception(x)
	{
	if (tss_proc())
		{
		fp = tss_proc()->fp;
		sp = GETSP();
		// prevent clear_unused from wiping exception info
		if (fp > tss_proc()->except_fp)
			tss_proc()->except_fp = fp;
		}
	else
		{
		fp = 0;
		sp = 0;
		}
	}

Ostream& operator<<(Ostream& os, const Except& x)
	{
	return os << x.exception;
	}

static OstreamStr os(200);

Ostream& osexcept()
	{
	return os;
	}

void except_()
	{
	Except x(gcstring(os.str()).trim().str());
	os.clear();
	throw x;
	}

// to allow setting breakpoints
void except_err_()
	{
	Except x(gcstring(os.str()).trim().str());
	os.clear();
	throw x;
	}
