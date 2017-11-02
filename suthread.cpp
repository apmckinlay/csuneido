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
// ReSharper disable once CppUnusedIncludeDirective
#include "dbms.h"
#include "alert.h"
#include "sublock.h"
#include "builtinclass.h"

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

class SuThread : public SuValue
	{
public:
	static auto methods()
		{
		return gsl::span<Method<SuThread>>(); // none
		}

	static Value Count(BuiltinArgs&);
	static Value List(BuiltinArgs&);
	static Value Name(BuiltinArgs&);
	static Value Sleep(BuiltinArgs&);
	};

template<>
Value BuiltinClass<SuThread>::instantiate(BuiltinArgs& args)
	{
	except("can't create instance of Thread");
	}

template<>
Value BuiltinClass<SuThread>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: Thread(func)");
	Value func = args.getValue("func");
	args.end();
	persist_if_block(func);
	Fibers::create(thread, new ThreadInfo(func));
	return Value();
	}

template<>
auto BuiltinClass<SuThread>::static_methods()
	{
	static StaticMethod methods[]
		{
		{ "Count", &SuThread::Count },
		{ "List", &SuThread::List },
		{ "Name", &SuThread::Name },
		{ "Sleep", &SuThread::Sleep }
		};
	return gsl::make_span(methods);
	}

Value SuThread::Count(BuiltinArgs& args)
	{
	args.usage("usage: Thread.Count()").end();
	return Fibers::size();
	}

Value SuThread::List(BuiltinArgs& args)
	{
	args.usage("usage: Thread.List()").end();
	SuObject* list = new SuObject();
	Fibers::foreach_fiber_info(
		[list](gcstring name, const char* status)
		{ list->putdata(new SuString(name), status); });
	return list;
	}

Value SuThread::Name(BuiltinArgs& args)
	{
	args.usage("usage: Thread.Name() or Thread.Name(string)");
	if (Value name = args.getValue("string", Value()))
		Fibers::set_name(name.gcstr());
	args.end();
	return new SuString(Fibers::get_name());
	}

Value SuThread::Sleep(BuiltinArgs& args)
	{
	args.usage("usage: Thread.Sleep(ms)");
	int ms = args.getint("ms");
	args.end();
	Fibers::sleep(ms);
	return Value();
	}

Value su_Thread()
	{
	static BuiltinClass<SuThread> instance("(func)");
	return &instance;
	}
