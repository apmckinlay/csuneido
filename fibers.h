#ifndef FIBERS_H
#define FIBERS_H

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

#include "gcstring.h"

struct Proc;

typedef void (*StackFn)(void* org, void* end);
typedef void (*ProcFn)(Proc* proc);

struct Fibers
	{
	static void init();

	// create a new fiber
	static void* create(void (_stdcall *fiber_proc)(void* arg), void* arg);

	// get current fiber
	static void* current();

	// get main fiber
	static void* main();

	// get main fibers dbms
	static class Dbms* main_dbms();

	// yield and don't run again till time has passed
	static void sleep(int ms = 20);

	// mark the fiber as blocked and yield
	static void block();

	// mark the fiber as runnable
	static void unblock(void* fiber);

	// if messages queued (including slice timer), switch fibers
	static void yieldif();

	// yield for sure, returns false if no runnable background fibers
	static bool yield();

	// mark current fiber as done and yield
	static void end();

	// delete ended fibers
	static void cleanup();

	// for garbage collection
	static void foreach_stack(StackFn fn);
	static void foreach_proc(ProcFn fn);

	// current number of fibers
	static int size();
	};

void sleepms(int ms);

struct Proc;
class Dbms;
class SesViews;

struct ThreadLocalStorage
	{
	ThreadLocalStorage();

	Proc* proc;
	Dbms* thedbms;
	SesViews* session_views;
	char* fiber_id;
	};

extern ThreadLocalStorage& tls();

#endif
