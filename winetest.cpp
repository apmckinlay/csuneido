#include "win.h"
#include <stdio.h>
#include <stdlib.h>

void fatal_log(const char* msg) {
	printf("%s\n", msg);
	exit(-1);
}

struct Region {
	void* start;
	void* end;
};

Region get_static_region() {
	char exefile[512];
	GetModuleFileName(NULL, exefile, sizeof(exefile));

	HANDLE hFile = CreateFile(exefile, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		fatal_log("get_static_region: unable to open exe");

	HANDLE hFileMapping =
		CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMapping == 0)
		fatal_log("get_static_region: unable to create mappping of exe");

	void* lpFileBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (lpFileBase == 0)
		fatal_log("get_static_region: unable to map view of exe");

	IMAGE_DOS_HEADER* dosHeader = (PIMAGE_DOS_HEADER) lpFileBase;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		fatal_log("get_static_region: bad magic");

	IMAGE_NT_HEADERS* pNTHeader =
		(PIMAGE_NT_HEADERS)((DWORD) dosHeader + dosHeader->e_lfanew);
	if (IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) ||
		pNTHeader->Signature != IMAGE_NT_SIGNATURE)
		fatal_log("get_static_region: unknown exe type");

	Region region;
	region.start = 0;
	IMAGE_SECTION_HEADER* section = (IMAGE_SECTION_HEADER*) (pNTHeader + 1);
	for (int i = 0; i < pNTHeader->FileHeader.NumberOfSections;
		 ++i, ++section) {
		printf("name: %s address: %x size: %d\n", section->Name,
			(int) section->VirtualAddress, (int) section->Misc.VirtualSize);
		if (0 == strcmp((char*) section->Name, ".data")) {
			region.start = (void*) (0x400000 + section->VirtualAddress);
			region.end =
				(void*) ((char*) region.start + section->Misc.VirtualSize);
			break;
		}
	}
	if (!region.start)
		fatal_log("get_static_region: couldn't find .data section");

	UnmapViewOfFile(lpFileBase);
	CloseHandle(hFileMapping);
	CloseHandle(hFile);

	return region;
}

void push_static_roots1(void (*push)(void*, void*)) {
	static Region region = get_static_region();
	printf("region %p -> %p\n", region.start, region.end);
	for (void* p = region.start; p < region.end;) {
		MEMORY_BASIC_INFORMATION m;
		VirtualQuery(p, &m, sizeof(m));
		void* end = (char*) p + m.RegionSize;
		// printf("vq start %p, end %p, commit %s, rw %s %d\n", p, end,
		// m.State == MEM_COMMIT ? "yes" : "no",
		// (m.Protect == PAGE_READWRITE || m.Protect == PAGE_EXECUTE_READWRITE)
		//? "yes" : "no", ~ m.Protect);
		if (m.State == MEM_COMMIT &&
			(m.Protect == PAGE_READWRITE ||
				m.Protect == PAGE_EXECUTE_READWRITE ||
				m.Protect == PAGE_WRITECOPY ||
				m.Protect == PAGE_EXECUTE_WRITECOPY))
			push(p, end);
		p = (char*) end;
	}
}

#if defined(__GNUC__)
extern "C" int _data_start__[];
extern "C" int _data_end__[];
extern "C" int _bss_start__[];
extern "C" int _bss_end__[];

void push_static_roots2(void (*push)(void*, void*)) {
	push(_data_start__, _data_end__);
	push(_bss_start__, _bss_end__);
}
#endif

void push(void* from, void* to) {
	printf("%p -> %p\n", from, to);
}

int big[10000] = {123};

int main(int argc, char** argv) {
	memset(big, sizeof big, 0);
	printf("big %p\n\n", big);
#if defined(__GNUC__)
	printf("GCC data & bss\n");
	push_static_roots2(push);
	printf("\n");
#endif
	printf("Win API:\n");
	push_static_roots1(push);
	return 0;
}
