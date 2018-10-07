// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "port.h"
#include "heap.h"
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "except.h"
#include "mmfile.h"
#include "fatal.h"
#include "ostreamstr.h"

void get_exe_path(char* buf, int buflen) {
	GetModuleFileName(nullptr, buf, buflen);
}

void* mem_committed(int n) {
	void* p =
		VirtualAlloc(nullptr, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return p;
}

void* mem_uncommitted(int n) {
	void* p =
		VirtualAlloc(nullptr, n, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
	verify(p);
	return p;
}

void mem_commit(void* p, int n) {
	verify(VirtualAlloc(p, n, MEM_COMMIT, PAGE_READWRITE));
}

void mem_decommit(void* p, int n) {
	VirtualFree(p, n, MEM_DECOMMIT);
}

void mem_release(void* p) {
	verify(VirtualFree(p, 0, MEM_RELEASE));
}

Heap::Heap(bool executable) {
	const int EXECUTABLE = 0x00040000;
	heap = HeapCreate(executable ? EXECUTABLE : 0,
		0,  // initial size
		0); // max size, 0 = growable
	if (!heap)
		except("HeapCreate failed");
}

void* Heap::alloc(int n) {
	verify(heap);
	void* p = HeapAlloc(heap, 0, n);
	verify(p);
	return p;
}

void Heap::free(void* p) {
	verify(heap);
	verify(HeapFree(heap, 0, p));
}

void Heap::destroy() {
	if (heap)
		HeapDestroy(heap);
	heap = nullptr;
}

Heap::~Heap() {
	destroy();
}

// WARNING: changes file position
// can't use GetFileSize because it's limited to 32 bit (4 gb) result
// can't use GetFileSizeEx because it's not available on older Windows
int64_t getfilesize(void* f) {
	LARGE_INTEGER li;
	li.QuadPart = 0;
	li.LowPart = SetFilePointer(f, li.LowPart, &li.HighPart, FILE_END);
	verify(li.LowPart != INVALID_FILE_SIZE);
	return li.QuadPart;
}

void Mmfile::open(const char* filename, bool create, bool ro) {
	f = CreateFile(filename, GENERIC_READ | (ro ? 0 : GENERIC_WRITE),
		FILE_SHARE_READ | (ro ? FILE_SHARE_WRITE : 0),
		nullptr, // no security attributes
		create ? OPEN_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
		nullptr); // no template
	if (f == INVALID_HANDLE_VALUE)
		except("can't open: " << filename);

	file_size = getfilesize(f);
}

// #include "ostreamfile.h"
// OstreamFile mlog("mmfile.log", "w");

void Mmfile::map(int chunk) {
	verify(!base[chunk]);

	int64_t end = (int64_t)(chunk + 1) * chunk_size;
	fm[chunk] = CreateFileMapping(f,
		nullptr, // no security attributes
		readonly ? PAGE_READONLY : PAGE_READWRITE, (unsigned) (end >> 32),
		(unsigned) (end & 0xffffffff),
		nullptr); // no name for mapping
	if (!fm[chunk])
		fatal("can't create file mapping for database");

	int64_t offset = (int64_t) chunk * chunk_size;
	base[chunk] = (char*) MapViewOfFile(fm[chunk],
		readonly ? FILE_MAP_READ : FILE_MAP_WRITE, (unsigned) (offset >> 32),
		(unsigned) (offset & 0xffffffff), chunk_size);
	// mlog << "+ " << chunk << " = " << (void*) base[chunk] << endl;

	if (!base[chunk]) {
		char buf[1000];
		int dw = GetLastError();
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof buf,
			nullptr);
		fatal("can't map view of database file", buf);
	}
}

void Mmfile::unmap(int chunk) {
	if (!base[chunk])
		return;
	// mlog << "- " << chunk << " = " << (void*) base[chunk] << endl;
	UnmapViewOfFile(base[chunk]);
	CloseHandle(fm[chunk]);
	base[chunk] = nullptr;
}

void Mmfile::sync() {
	for (int i = 0; i <= hi_chunk; ++i)
		if (base[i])
			FlushViewOfFile(base[i], 0); // 0 means all
}

Mmfile::~Mmfile() {
	for (int i = 0; i <= hi_chunk; ++i)
		unmap(i);

	if (!readonly) {
		// adjust file size
		LARGE_INTEGER li;
		li.QuadPart = file_size;
		SetFilePointer(f, li.LowPart, &li.HighPart, FILE_BEGIN);
		SetEndOfFile(f);
		verify(file_size == getfilesize(f));
	}

	CloseHandle(f);
}

static Mmfile* mmf;

static VOID CALLBACK SyncTimerProc(
	HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	mmf->sync();
}

void sync_timer(Mmfile* m) {
	mmf = m;
	SetTimer(nullptr, // handle to main window
		0,            // timer identifier
		60000,        // 1-minute interval
		(TIMERPROC) SyncTimerProc);
}

#include <process.h>
#include "cmdlineoptions.h"

int fork_rebuild() {
	char exefile[1024];
	GetModuleFileName(nullptr, exefile, sizeof exefile);
	OstreamStr args(20);
	args << "-r";
	if (cmdlineoptions.unattended)
		args << " -u";
	if (cmdlineoptions.check_start)
		args << " -cs";
	return _spawnl(_P_WAIT, exefile, "suneido.exe", args.str(), NULL);
}

#include "date.h"

DateTime::DateTime() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	year = st.wYear;
	month = st.wMonth;
	day = st.wDay;
	hour = st.wHour;
	minute = st.wMinute;
	second = st.wSecond;
	millisecond = st.wMilliseconds;
}

static void (*dbstp_pfn)();

static VOID CALLBACK DbServerTimerProc(
	HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	(*dbstp_pfn)();
}

void dbserver_timer(void (*pfn)()) {
	dbstp_pfn = pfn;
	SetTimer(nullptr, // handle to main window
		0,            // timer identifier
		60000,        // 1 minute interval
		(TIMERPROC) DbServerTimerProc);
}
