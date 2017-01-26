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
	enum Status { READY, BLOCKED, SLEEPING, ENDED, REUSE };
	explicit Fiber(void* f, void* arg = 0)
		: fiber(f), status(READY), priority(0), stack_ptr(0), stack_end(0),
		arg_ref(arg)
		{ }
	bool operator==(Status s)
		{ return status == s; }
	void* fiber;
	Status status;
	char* id;
	int priority; // 0 = normal, -1 = low (dbcopy)
	// for garbage collector
	void* stack_ptr;
	void* stack_end;
	void* arg_ref; // prevent it being garbage collected
	ThreadLocalStorage tls;
	};

static Fiber main_fiber(0);
static std::vector<Fiber> fibers;
const int MAIN = -1;
static int cur = MAIN;
#define curfiber (cur < 0 ? &main_fiber : &fibers[cur])

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

void CALLBACK timerproc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
	{
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status == Fiber::SLEEPING)
			fibers[i].status = Fiber::READY;
	}

void Fibers::init()
	{
	main_fiber.fiber = ConvertThreadToFiber(0);
	verify(main_fiber.fiber);

	// time slice timer
	SetTimer(NULL,	// hwnd
		0,		// id
		50,		// ms
		timerproc
		);

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
	save_stack();
	
	cur = i;
	SwitchToFiber(fiber.fiber);
	}

void Fibers::yieldif()
	{
	// only yield if any messages waiting
	// this includes time slice events
	if (curfiber != &main_fiber && HIWORD(GetQueueStatus(QS_ALLEVENTS)))
		switchto(MAIN);
	}

void Fibers::sleep()
	{
	verify(current() != main());
	curfiber->status = Fiber::SLEEPING;
	yield();
	}

void Fibers::yield()
	{
	static int f = -1; // round robin index into fibers

	bool low = false;
	int nfibers = fibers.size();
	int i;
	for (i = 0; i < nfibers; ++i)
		{
		f = (f + 1) % nfibers;
		if (fibers[f].priority < 0)
			{
			low = true;
			continue ;
			}
		if (fibers[f].status == Fiber::READY)
			{
			switchto(f);
			return ;
			}
		}
	if (low)
		for (i = 0; i < nfibers; ++i)
			{
			f = (f + 1) % nfibers;
			if (fibers[f].priority >= 0)
				continue ;
			if (fibers[f].status == Fiber::READY)
				{
				switchto(f);
				return ;
				}
			}
	// no runnable fibers
	switchto(MAIN);
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

void Fibers::end()
	{
	verify(current() != main());
	curfiber->status = Fiber::ENDED;
	yield();
	unreachable();
	}

// called by main fiber message loop
void Fibers::cleanup()
	{
#ifndef __GNUC__
	verify(current() == main());
	for (int i = 0; i < fibers.size(); ++i)
		{
		if (i != cur && fibers[i].status == Fiber::ENDED)
			{
			DeleteFiber(fibers[i].fiber);
			memset(&fibers[i], 0, sizeof Fiber);
			fibers[i].status = Fiber::REUSE;
			}
		}
#endif
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

void Fibers::priority(int p)
	{
	verify(p == 0 || p == -1);
	curfiber->priority = p;
	}

int Fibers::size()
	{
	int n = 0;
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status < Fiber::ENDED)
			++n;
	return n;
	}