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
 * If there are any message on the Windows event queue 
 * we switch to the main fiber to handle them.
 * The main fiber yields to the next runnable background fiber.
 * Background fibers always yield to the main fiber (not each other).
 * interp and database queries call yieldif regularly.
 * This will yield if either a message is on the Windows event queue
 * or if the time slice has expired.
 */

#include "fibers.h"
#include "except.h"
#include "itostr.h" // for gc warn
#include "errlog.h" // for gc warn
#include "gcstring.h"
#include "win.h"
#include "winlib.h"
#include <vector>
#include <algorithm>

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

static Fiber main_fiber(0);
static std::vector<Fiber> fibers;
const int MAIN = -1;
static int cur = MAIN;
#define curfiber (cur < 0 ? &main_fiber : &fibers[cur])
static int64 last_switch;
static int64 qpc_freq;

static const int TIME_SLICE_MS = 50;

ThreadLocalStorage::ThreadLocalStorage() 
	: proc(0), thedbms(0), session_views(0), fiber_id("")
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

inline int64 qpf()
	{
	LARGE_INTEGER f;
	verify(QueryPerformanceFrequency(&f));
	return f.QuadPart;
	}

inline int64 qpc()
	{
	LARGE_INTEGER t;
	verify(QueryPerformanceCounter(&t));
	return t.QuadPart;
	}

void Fibers::init()
	{
	main_fiber.fiber = ConvertThreadToFiber(0);
	verify(main_fiber.fiber);

	qpc_freq = qpf();

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
	last_switch = qpc();
	save_stack();
	
	cur = i;
	SwitchToFiber(fiber.fiber);
	}

bool message_on_event_queue()
	{
	return HIWORD(GetQueueStatus(QS_ALLEVENTS));
	}

bool time_slice_finished()
	{
	auto t = qpc();
	auto d = t - last_switch; // elapsed time in ticks
	auto ms = (d * 1000) / qpc_freq; // elapsed time in milliseconds
	return ms > TIME_SLICE_MS;
	}

void Fibers::yieldif()
	{
	if (message_on_event_queue() || time_slice_finished())
		yield();
	}

void Fibers::sleep(int ms)
	{
	verify(current() != main());
	auto t = qpc();
	curfiber->sleep_until = t + ((ms * qpc_freq) / 1000);
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
	static int f = -1; // round robin index into fibers

	if (current() != main())
		{
		switchto(MAIN);
		return true;
		}

	int nfibers = fibers.size();
	int i;
	for (i = 0; i < nfibers; ++i)
		{
		f = (f + 1) % nfibers;
		if (fibers[f].status == Fiber::ENDED)
			{
			DeleteFiber(fibers[f].fiber);
			memset(&fibers[f], 0, sizeof (Fiber));
			fibers[f].status = Fiber::REUSE;
			}
		else if (runnable(fibers[f]))
			{
			switchto(f);
			return true;
			}
		}
	// no runnable fibers
	return false;
	}

void* Fibers::current()
	{
	return curfiber->fiber;
	}

void* Fibers::main()
	{
	return main_fiber.fiber;
	}

Dbms* Fibers::main_dbms()
	{
	return main_fiber.tls.thedbms;
	}

void Fibers::block()
	{
	verify(current() != main());
	curfiber->status = Fiber::BLOCKED;
	yield();
	}

//TODO change block to return index and unblock to take index, to avoid search
void Fibers::unblock(void* fiber)
	{
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].fiber == fiber)
			{
			verify(fibers[i].status == Fiber::BLOCKED);
			fibers[i].status = Fiber::READY;
			return ;
			}
	error("unblock didn't find fiber");
	}

[[noreturn]] void Fibers::end()
	{
	verify(current() != main());
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