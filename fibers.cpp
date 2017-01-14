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

/*
 * Cooperative (not preemptive) multi-tasking.
 * i.e. fibers must explicitly yield 
 * Priority is given to the main fiber which runs the UI message loop.
 * The main fiber yields to the next runnable background fiber.
 * Background fibers always yield to the main fiber (not each other).
 * interp and database queries call yieldif regularly.
 * This will yield if the time slice has expired or
 * if an io completion occurs.
 */

#include "fibers.h"
#include "except.h"
#include "itostr.h" // for gc warn
#include "errlog.h" // for gc warn
#include "win.h"
#include <vector>
#include "qpc.h"

struct Proc;
class Dbms;
class SesViews;

struct Fiber
	{
	enum Status { READY, BLOCKED, ENDED, REUSE };
	explicit Fiber(void* f, void* arg = 0)
		: fiber(f), status(READY), stack_ptr(0), stack_end(0), arg_ref(arg), sleep_until{0}
		{
		}
	bool operator==(Status s) const
		{ return status == s; }
	void* fiber;
	Status status;
	// for garbage collector
	void* stack_ptr;
	void* stack_end;
	void* arg_ref; // prevent it being garbage collected
	ThreadLocalStorage tls;
	int64 sleep_until;
	};

inline int64 qpfreq()
	{
	LARGE_INTEGER f;
	verify(QueryPerformanceFrequency(&f));
	return f.QuadPart;
	}
int64 qpf = qpfreq();

static Fiber main_fiber(0);
static std::vector<Fiber> fibers;
const int MAIN = -1;
static int cur = MAIN;
#define curfiber (cur < 0 ? &main_fiber : &fibers[cur])
static int fi = -1; // round robin index into fibers
static int64 run_until;

static const int TIME_SLICE_MS = 50;

ThreadLocalStorage::ThreadLocalStorage() 
	: proc(0), thedbms(0), session_views(0), fiber_id(""), synchronized(0)
	{ }

ThreadLocalStorage& tls()
	{
	return curfiber->tls;
	}

static void save_stack()
	{
	if (curfiber->stack_end == 0)
		{
		void* p;
#if defined(_MSC_VER)
		_asm
			{
			mov eax,fs:4
			mov p,eax
			}
#elif defined(__GNUC__)
		asm("movl %%fs:0x4,%%eax" : "=a" (p));
#else
#warning "replacement for inline assembler required"
#endif
		curfiber->stack_end = p;
		}
	int junk;
	curfiber->stack_ptr = &junk;
	}

void clear_unused();

extern "C"
	{
	void GC_push_all_stack(void* bottom, void* top);
	void GC_push_all_stacks()
		{
		Fibers::foreach_stack(GC_push_all_stack);
		clear_unused();
		}
	void GC_push_thread_structures()
		{
		}
	
	typedef void (*GC_warn_proc)(char* msg, unsigned long arg);
	GC_warn_proc GC_set_warn_proc(GC_warn_proc fn);
	void warn(char* msg, unsigned long arg)
		{
		const char* limit = "GC Info: Too close to address space limit: blacklisting ineffective";
		if (0 == memcmp(msg, limit, strlen(limit)))
			return ;
		
		const char* repeat = "GC Warning: Repeated allocation of very large block";
		static int nrepeats = 0;
		static int maxarg = 0;
		
		char extra[32] = "";
		if (0 == memcmp(msg, repeat, strlen(repeat)))
			{
			++nrepeats;
			if (arg <= maxarg)
				return ;
			maxarg = arg;
			extra[0] = '(';
			utostr(nrepeats, extra + 1);
			strcat(extra, " repeats)");
			}	
		char buf[32];
		errlog(msg, utostr(arg, buf), extra);
		}
	};

void Fibers::init()
	{
	main_fiber.fiber = ConvertThreadToFiber(0);
	verify(main_fiber.fiber);

	GC_set_warn_proc(warn);
	}

void* Fibers::create(void (_stdcall *fiber_proc)(void* arg), void* arg)
	{
	void* f = CreateFiber(0, fiber_proc, arg);
	verify(f);
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status == Fiber::REUSE)
			{
			fibers[i] = Fiber(f, arg);
			return f;
			}
	// else
	fibers.push_back(Fiber(f, arg));
	return f;
	}

static void switchto(int i)
	{
	Fiber& fiber = (i == MAIN ? main_fiber : fibers[i]);
	verify(fiber.status == Fiber::READY);
	if (i == cur)
		return ;
	run_until = qpc() + ((TIME_SLICE_MS * qpf) / 1000);
	save_stack();
	
	cur = i;
	SwitchToFiber(fiber.fiber);
	}

void Fibers::yieldif()
	{
	if (tls().synchronized == 0 && qpc() > run_until)
		yield();
	}

void Fibers::sleep(int ms)
	{
	verify(!inMain());
	curfiber->sleep_until = qpc() + ((ms * qpf) / 1000);
	yield();
	}

bool runnable(Fiber& f)
	{
	if (f.status != Fiber::READY)
		return false;
	if (f.sleep_until == 0)
		return true;
	auto t = qpc();
	if (t < f.sleep_until)
		return false;
	f.sleep_until = 0;
	return true;
	}

bool Fibers::yield()
	{
	if (!inMain())
		{
		switchto(MAIN);
		return true;
		}

	int nfibers = fibers.size();
	int i;
	for (i = 0; i < nfibers; ++i)
		{
		fi = (fi + 1) % nfibers;
		if (fibers[fi].status == Fiber::ENDED)
			{
			DeleteFiber(fibers[fi].fiber);
			memset(&fibers[fi], 0, sizeof (Fiber));
			fibers[fi].status = Fiber::REUSE;
			}
		else if (runnable(fibers[fi]))
			{
			switchto(fi);
			return true;
			}
		}
	// no runnable fibers
	return false;
	}

bool Fibers::inMain()
	{
	return curfiber == &main_fiber;
	}

Dbms* Fibers::main_dbms()
	{
	return main_fiber.tls.thedbms;
	}

int Fibers::curFiberIndex()
	{
	return cur;
	}

void Fibers::block()
	{
	verify(!inMain());
	curfiber->status = Fiber::BLOCKED;
	yield();
	}

void Fibers::unblock(int fiberIndex)
	{
	verify(0 <= fiberIndex && fiberIndex <= fibers.size());
	verify(fibers[fiberIndex].status == Fiber::BLOCKED);
	fibers[fiberIndex].status = Fiber::READY;
	fi = fiberIndex - 1; // so it will run next (see yield)
	}

[[noreturn]] void Fibers::end()
	{
	verify(!inMain());
	curfiber->status = Fiber::ENDED;
	yield();
	unreachable();
	}

void Fibers::foreach_stack(StackFn fn)
	{
	save_stack();
	fn(main_fiber.stack_ptr, main_fiber.stack_end);
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status < Fiber::ENDED)
			fn(fibers[i].stack_ptr, fibers[i].stack_end);
	}

void Fibers::foreach_proc(ProcFn fn)
	{
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status < Fiber::ENDED)
			fn(fibers[i].tls.proc);
	}

void sleepms(int ms)
	{
	Sleep(ms); 
	}

int Fibers::size()
	{
	int n = 0;
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status < Fiber::ENDED)
			++n;
	return n;
	}