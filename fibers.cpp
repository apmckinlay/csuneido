// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

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
#include "qpc.h"
#include <functional>
#include "gcstring.h"
#include "ostreamstr.h"

//#include "ostreamcon.h"
//#define LOG(stuff) con() << stuff << endl
#define LOG(stuff)

static int fiber_count = 0;

struct Fiber {
	enum Status { READY, BLOCKED, REUSE };
	Fiber() : status(REUSE) {
	}
	explicit Fiber(void* f, void* arg = nullptr)
		: fiber(f), status(READY), arg_ref(arg) {
		fiber_number = ++fiber_count;
	}
	bool operator==(Status s) const {
		return status == s;
	}
	void* fiber = nullptr;
	Status status;
	gcstring name;
	int fiber_number = 0;
	// for garbage collector
	void* stack_ptr = nullptr;
	void* stack_end = nullptr;
	void* arg_ref = nullptr; // prevent it being garbage collected
	int64_t sleep_until = 0;
	ThreadLocalStorage tls;
};

inline int64_t qpfreq() {
	LARGE_INTEGER f;
	verify(QueryPerformanceFrequency(&f));
	return f.QuadPart;
}
int64_t qpf = qpfreq();

const int MAXFIBERS = 32;
static Fiber fibers[MAXFIBERS];
const int MAIN = 0;
static Fiber* cur = &fibers[MAIN];
static int fi = 0; // round robin index into fibers, used by yield
static int64_t run_until;

static const int TIME_SLICE_MS = 50;

ThreadLocalStorage::ThreadLocalStorage()
	: proc(0), thedbms(0), session_views(0), fiber_id(""), synchronized(0) {
}

ThreadLocalStorage& tls() {
	return cur->tls;
}

static void save_stack() {
	if (cur->stack_end == 0) {
		void* p;
#if defined(_MSC_VER)
		_asm {
			mov eax,fs:4
			mov p,eax
		}
#elif defined(__GNUC__)
		asm("movl %%fs:0x4,%%eax" : "=a"(p));
#else
#warning "replacement for inline assembler required"
#endif
		cur->stack_end = p;
	}
	int junk;
	cur->stack_ptr = &junk;
}

void clear_proc(Proc* proc);

typedef void (*StackFn)(void* org, void* end);
typedef void (*ProcFn)(Proc* proc);

/// for garbage collection
static void foreach_stack(StackFn fn);
static void foreach_proc(ProcFn fn);

extern "C" {
void GC_push_all_stack(void* bottom, void* top);
void GC_push_all_stacks() {
	foreach_stack(GC_push_all_stack);
	foreach_proc(clear_proc);
}
void GC_push_thread_structures() {
}

typedef void (*GC_warn_proc)(char* msg, unsigned int arg);
GC_warn_proc GC_set_warn_proc(GC_warn_proc fn);
void warn(char* msg, unsigned int arg) {
	const char* limit =
		"GC Info: Too close to address space limit: blacklisting ineffective";
	if (0 == memcmp(msg, limit, strlen(limit)))
		return;

	const char* repeat = "GC Warning: Repeated allocation of very large block";
	static int nrepeats = 0;
	static int maxarg = 0;

	char extra[32] = "";
	if (0 == memcmp(msg, repeat, strlen(repeat))) {
		++nrepeats;
		if (arg <= maxarg)
			return;
		maxarg = arg;
		extra[0] = '(';
		utostr(nrepeats, extra + 1);
		strcat(extra, " repeats)");
	}
	char buf[32];
	errlog(msg, utostr(arg, buf), extra);
}
};

void Fibers::init() {
	void* f = ConvertThreadToFiber(nullptr);
	verify(f);

	verify(Fibers::curFiberIndex() == MAIN);
	cur->fiber = f;
	cur->status = Fiber::READY;

	GC_set_warn_proc(warn);
}

static void deleteFiber(Fiber& f, int i, const char* from_fn) {
	LOG("delete fiber " << i << " from " << from_fn);
	verify(&f != cur);
	DeleteFiber(f.fiber);
	f = Fiber(); // to help garbage collection
}

void Fibers::create(void(_stdcall* fiber_proc)(void* arg), void* arg) {
	for (int i = 1; i < MAXFIBERS; ++i) {
		Fiber& f = fibers[i];
		if (f.status == Fiber::REUSE && f.fiber)
			deleteFiber(f, i, "create");
	}
	void* f = CreateFiber(0, fiber_proc, arg);
	if (!f)
		except("can't create fiber, possibly low on resources");
	for (int i = 1; i < MAXFIBERS; ++i)
		if (fibers[i].status == Fiber::REUSE) {
			LOG("create " << i);
			fibers[i] = Fiber(f, arg);
			return;
		}
	// else
	except("can't create fiber, max is " << MAXFIBERS);
}

static void switchto(int i) {
	LOG("start switchto " << i);
	Fiber& fiber = fibers[i];
	if (&fiber == cur)
		return;
	LOG("switching");
	verify(fiber.status == Fiber::READY);
	save_stack();
	cur = &fiber;
	run_until = qpc() + ((TIME_SLICE_MS * qpf) / 1000);
	SwitchToFiber(fiber.fiber);
}

void Fibers::yieldif() {
	if (tls().synchronized == 0 &&
		(qpc() > run_until || SleepEx(0, true) == WAIT_IO_COMPLETION))
		yield();
}

void Fibers::sleep(int ms) {
	verify(!inMain());
	cur->sleep_until = qpc() + ((ms * qpf) / 1000);
	yield();
}

static bool runnable(const Fiber& f) {
	if (f.status != Fiber::READY)
		return false;
	if (f.sleep_until == 0)
		return true;
	return f.sleep_until <= qpc();
}

bool Fibers::yield() {
	if (!inMain()) {
		switchto(MAIN);
		return true;
	}

	for (int i = 1; i < MAXFIBERS; ++i) {
		fi = fi % (MAXFIBERS - 1) + 1;
		Fiber& f = fibers[fi];
		if (f.status == Fiber::REUSE && f.fiber)
			deleteFiber(f, fi, "yield");
		else if (runnable(f)) {
			switchto(fi);
			return true;
		}
	}
	// no runnable fibers
	return false;
}

bool Fibers::inMain() {
	return cur == &fibers[0];
}

Dbms* Fibers::main_dbms() {
	return fibers[MAIN].tls.thedbms;
}

int Fibers::curFiberIndex() {
	return cur - &fibers[0];
}

void Fibers::block() {
	verify(!inMain());
	cur->status = Fiber::BLOCKED;
	yield();
}

void Fibers::unblock(int fiberIndex) {
	verify(1 <= fiberIndex && fiberIndex < MAXFIBERS);
	verify(fibers[fiberIndex].status == Fiber::BLOCKED);
	fibers[fiberIndex].status = Fiber::READY;
	fi = fiberIndex - 1; // so it will run next (see yield)
}

void Fibers::end() {
	LOG("end " << curFiberIndex());
	verify(!inMain());
	cur->status = Fiber::REUSE;
	yield();
	unreachable();
}

static void foreach_fiber(std::function<void(Fiber&)> f) {
	for (int i = 0; i < MAXFIBERS; ++i)
		if (fibers[i].status != Fiber::REUSE)
			f(fibers[i]);
}

void Fibers::foreach_tls(std::function<void(ThreadLocalStorage&)> f) {
	foreach_fiber([f](Fiber& fiber) { f(fiber.tls); });
}

static void foreach_stack(StackFn fn) {
	save_stack();
	foreach_fiber([fn](Fiber& fiber) { fn(fiber.stack_ptr, fiber.stack_end); });
}

static void foreach_proc(ProcFn fn) {
	foreach_fiber([fn](Fiber& fiber) { fn(fiber.tls.proc); });
}

void sleepms(int ms) {
	Sleep(ms);
}

int Fibers::size() {
	int n = 0;
	foreach_fiber([&n](Fiber&) { ++n; });
	return n - 1; // exclude main fiber
}

static gcstring build_fiber_name(const gcstring& name, int fiber_number) {
	OstreamStr os;
	os << "Thread-" << fiber_number;
	if (name != "")
		os << " " << name;
	return os.gcstr();
}

gcstring Fibers::get_name() {
	return build_fiber_name(cur->name, cur->fiber_number);
}

void Fibers::set_name(const gcstring& name) {
	cur->name = name;
}

void Fibers::foreach_fiber_info(
	std::function<void(const gcstring&, const char*)> fn) {
	foreach_fiber([fn](Fiber& fiber) {
		if (&fibers[MAIN] == &fiber)
			return;
		gcstring fiber_name = build_fiber_name(fiber.name, fiber.fiber_number);
		fn(fiber_name, fiber.status == Fiber::READY ? "READY" : "BLOCKED");
	});
}

const char* Fibers::default_sessionid() {
	if (inMain())
		return "";
	OstreamStr os;
	os << fibers[MAIN].tls.fiber_id << ":" << Fibers::get_name();
	return os.str();
}
