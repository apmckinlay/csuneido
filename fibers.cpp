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
#include "win.h"
#include "winlib.h"
#include <vector>
#include <algorithm>
#include "tls.h"

// use function pointers to allow loading from kernel32 or w95fiber

typedef void* (_stdcall *pCTTF)(void* data);
static pCTTF MyConvertThreadToFiber;

typedef void (CALLBACK *FIBER_PROC)(void* param);

typedef void* (_stdcall *pCF)(DWORD dwStackSize, FIBER_PROC start, void* param);
static pCF MyCreateFiber;

typedef void (_stdcall *pSTF)(void* fiber);
static pSTF MySwitchToFiber;

typedef void (_stdcall *pDF)(void* fiber);
static pDF MyDeleteFiber;

struct Proc;
class Dbms;

const int MAX_TLS = 8;
static void** tls_vars[MAX_TLS];  // addresses of static tls variable
static int n_tls_vars = 0;

void register_tls(void** pvar)
	{
	verify(n_tls_vars < MAX_TLS);
	tls_vars[n_tls_vars++] = pvar;
	}

struct Fiber
	{
	enum Status { READY, BLOCKED, ENDED };
	explicit Fiber(void* f) 
		: fiber(f), status(READY), proc(0), dbms(0), priority(0), stack_ptr(0), stack_end(0)
		{
		std::fill(tls, tls + MAX_TLS, (void*) 0);
		}
	bool operator==(Status s)
		{ return status == s; }
	void* fiber;
	Status status;
	Proc* proc;
	Dbms* dbms;
	char* id;
	int priority; // 0 = normal, -1 = low (dbcopy)
	// for garbage collector
	void* stack_ptr;
	void* stack_end;
	// thread/fiber local storage
	void* tls[MAX_TLS];
	};

static Fiber main_fiber(0);
static std::vector<Fiber> fibers;
static Fiber* curfiber = &main_fiber;

static void* getproc(char* name)
	{
	static WinLib lib("kernel32");

	void* p = lib.GetProcAddress(name);
	if (! p)
		{
		lib.retarget("w95fiber");
		p = lib.GetProcAddress(name);
		}
	if (! p)
		except("can't load " << name);
	return p;
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
#elif
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
	verify(! MyCreateFiber);

	MyConvertThreadToFiber = (pCTTF) getproc("ConvertThreadToFiber");
	MyCreateFiber = (pCF) getproc("CreateFiber");
	MySwitchToFiber = (pSTF) getproc("SwitchToFiber");
	MyDeleteFiber = (pDF) getproc("DeleteFiber");

	main_fiber.fiber = MyConvertThreadToFiber(0);
	verify(main_fiber.fiber);
	curfiber = &main_fiber;

	// time slice timer
	SetTimer(NULL,	// hwnd
		0,			// id
		100,		// ms
		NULL		// no timer proc - just generate messages
		);

	GC_set_warn_proc(warn);
	}

void* Fibers::create(void (_stdcall *fiber_proc)(void* arg), void* arg)
	{
	verify(MyCreateFiber);

	void* f = MyCreateFiber(0, fiber_proc, arg);
	verify(f);
	fibers.push_back(Fiber(f));
	return f;
	}

char* fiber_id = "";
TLS(fiber_id);

static void switchto(Fiber& fiber)
	{
	verify(fiber.status == Fiber::READY);
	if (&fiber == curfiber)
		return ;
	save_stack();
	
	for (int i = 0; i < n_tls_vars; ++i)
		curfiber->tls[i] = *(tls_vars[i]);
	
	for (int i = 0; i < n_tls_vars; ++i)
		*(tls_vars[i]) = fiber.tls[i];
	
	curfiber = &fiber;
	MySwitchToFiber(fiber.fiber);
	}

void Fibers::yieldif()
	{
	// only yield if any messages waiting
	// this includes time slice events
	if (curfiber != &main_fiber && HIWORD(GetQueueStatus(QS_ALLEVENTS)))
		switchto(main_fiber);
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
			switchto(fibers[f]);
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
				switchto(fibers[f]);
				return ;
				}
			}
	// no runnable fibers
	switchto(main_fiber);
	}

void* Fibers::current()
	{
	return curfiber->fiber;
	}

void* Fibers::main()
	{
	return main_fiber.fiber;
	}

void Fibers::block()
	{
	verify(current() != main());
	curfiber->status = Fiber::BLOCKED;
	yield();
	}

void Fibers::unblock(void* fiber)
	{
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].fiber == fiber)
			{ fibers[i].status = Fiber::READY; return ; }
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
	for (int i = fibers.size() - 1; i >= 0; --i)
		{
		if (fibers[i].status == Fiber::ENDED)
			{
			MyDeleteFiber(fibers[i].fiber);
			fibers.erase(fibers.begin() + i);
			}
		}
#endif
	}

void Fibers::foreach_stack(StackFn fn)
	{
	save_stack();
	fn(main_fiber.stack_ptr, main_fiber.stack_end);
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status != Fiber::ENDED)
			fn(fibers[i].stack_ptr, fibers[i].stack_end);
	}

void Fibers::foreach_proc(ProcFn fn)
	{
	extern Proc* proc;

	fn(proc);
	for (int i = 0; i < fibers.size(); ++i)
		if (fibers[i].status != Fiber::ENDED)
			fn(fibers[i].proc);
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
