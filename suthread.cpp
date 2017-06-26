/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2016 Suneido Software Corp.
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

#include "value.h"
#include "fibers.h"
#include "errlog.h"
#include "exceptimp.h"
#include "win.h"
#include "suobject.h"
#include "interp.h"
#include "dbms.h"
#include "func.h"
#include "alert.h"
#include "suclass.h"
#include "sublock.h"

class ThreadClass : public SuValue
	{
public:
	Value call(Value self, Value member,
		short nargs, short nargnames, ushort* argnames, int each) override;
	void out(Ostream& os) const override
		{
		os << "Thread";
		}
	};

#pragma warning(disable:4722) // destructor never returns
struct ThreadCloser
	{
	ThreadCloser() // to avoid gcc unused warning
		{ }
	~ThreadCloser()
		{
		delete tls().thedbms;
		tls().thedbms = nullptr;

		Fibers::end();
		}
	};

struct ThreadInfo
	{
	ThreadInfo(Value f) : fn(f)
		{ }
	Value fn;
	};

// this is a wrapper that runs inside the fiber to catch exceptions
static void _stdcall thread(void* arg)
	{
	Proc p; tls().proc = &p;

	ThreadCloser closer;
	try
		{
		ThreadInfo* ti = static_cast<ThreadInfo*>(arg);
		ti->fn.call(ti->fn, CALL);
		}
	catch (const Except& e)
		{
		errlog("ERROR uncaught in thread:", e.str(), e.callstack());
		}
	}

Value ThreadClass::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Count("Count");
	static Value List("List");
	static Value Name("Name");
	static Value Sleep("Sleep");

	argseach(nargs, nargnames, argnames, each);

	if (member == CALL || member == INSTANTIATE)
		{
		if (nargs != 1)
			except("usage: Thread(callable)");
		persist_if_block(ARG(0));
		Fibers::create(thread, new ThreadInfo(ARG(0)));
		return Value();
		}
	else if (member == Count)
		{
		NOARGS("Thread.Count()");
		return Fibers::size();
		}
	else if (member == List)
		{
		NOARGS("Thread.List()");
		SuObject* list = new SuObject();
		Fibers::foreach_fiber_info(
			[list](gcstring name, const char* status) { list->putdata(name.str(), status); });
		return list;
		}
	else if (member == Name)
		{
		if (nargs > 1)
			except("usage: Thread.Name(name = false)");
		if (nargs == 0)
			return Fibers::get_name().str();
		const char* name = ARG(0).str();
		gcstring fiber_name = Fibers::set_name(name);
		return fiber_name.str();
		}
	else if (member == Sleep)
		{
		if (nargs != 1)
			except("usage: Thread.Sleep(milliseconds)");
		Fibers::sleep(ARG(0).integer());
		return Value();
		}
	else
		return RootClass::notfound(self, member, nargs, nargnames, argnames, each);
	}

Value thread_singleton()
	{
	static ThreadClass* instance = new ThreadClass;
	return instance;
	}
