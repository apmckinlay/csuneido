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

#include "suwinres.h"
#include "except.h"
#include "value.h"
#include "win.h"
#include "noargs.h"

SuWinRes::SuWinRes(void* handle) : h(handle)
	{ }

Value SuWinRes::call(Value self, Value member,
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Close("Close");

	if (member == Close)
		{
		NOARGS("handle.Close()");
		removefinal();
		return close() ? SuTrue : SuFalse;
		}
	else
		return SuValue::call(self, member, nargs, nargnames, argnames, each);
	}

void SuWinRes::finalize()
	{
	close();
	}

//===================================================================

SuHandle::SuHandle(void* handle) : SuWinRes(handle)
	{ }

void SuHandle::out(Ostream& os) const
	{
	os << "handle " << h;
	}

bool SuHandle::close()
	{
	bool ok = h && CloseHandle((HANDLE) h);
	h = 0;
	return ok;
	}

//===================================================================

SuGdiObj::SuGdiObj(void* handle) : SuWinRes(handle)
	{ }

void SuGdiObj::out(Ostream& os) const
	{
	os << "gdiobj " << h;
	}

int SuGdiObj::integer() const
	{
	return (int) h;
	}

bool SuGdiObj::close()
	{
	bool ok = h && DeleteObject((HGDIOBJ) h);
	h = 0;
	return ok;
	}
