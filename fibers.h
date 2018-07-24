#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <functional>
#include "gcstring.h"

struct Proc;
class Dbms;
class SesViews;

struct ThreadLocalStorage {
	ThreadLocalStorage();

	Proc* proc;
	Dbms* thedbms;
	SesViews* session_views;
	const char* fiber_id;
	int synchronized; // normally 0 (meaning allow yield), set by Synchronized
};

extern ThreadLocalStorage& tls();

struct Fibers {
	static void init();

	/// create a new fiber
	static void create(void(_stdcall* fiber_proc)(void* arg), void* arg);

	/// @return Whether currently running the main fiber
	static bool inMain();

	/// get main fibers dbms
	static class Dbms* main_dbms();

	/// yield and don't run again till time has passed
	static void sleep(int ms = 20);

	/// @return The index of the current fiber, to use with unblock
	static int curFiberIndex();

	/// Mark the fiber as blocked and yield
	/// i.e. Will not return until another fiber unblocks this fiber.
	static void block();

	/// mark the fiber as runnable
	static void unblock(int fiberIndex);

	/// if messages queued (including slice timer), switch fibers
	static void yieldif();

	/// yield for sure, returns false if no runnable background fibers
	static bool yield();

	/// mark current fiber as done and yield
	[[noreturn]] static void end();

	static void foreach_tls(std::function<void(ThreadLocalStorage&)> f);

	/// current number of fibers
	static int size();

	/// get thread name
	static gcstring get_name();

	/// set thread name
	static void set_name(const gcstring& name);

	/// list all threads
	static void foreach_fiber_info(
		std::function<void(const gcstring&, const char*)> fn);

	/// fiber's default session id
	static const char* default_sessionid();
};

void sleepms(int ms);
