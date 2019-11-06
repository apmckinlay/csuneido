#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "io.h"
#include "fcntl.h"
#include <cstdio>
#include <cstdlib>

void connectToConsole(bool repl) {
	bool attachStdIn = repl;
	bool attachStdout = true;
	bool attachStdErr = true;
	bool attachedToConsole = false;

	if (HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE)) {
		DWORD dwFileType = GetFileType(stdinHandle);
		if (dwFileType == FILE_TYPE_DISK || dwFileType == FILE_TYPE_PIPE)
			attachStdIn = false;
	}

	if (HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE)) {
		DWORD dwFileType = GetFileType(stdoutHandle);
		if (dwFileType == FILE_TYPE_DISK || dwFileType == FILE_TYPE_PIPE)
			attachStdout = false;
	}

	if (HANDLE stderrHandle = GetStdHandle(STD_ERROR_HANDLE)) {
		DWORD dwFileType = GetFileType(stderrHandle);
		if (dwFileType == FILE_TYPE_DISK || dwFileType == FILE_TYPE_PIPE)
			attachStdErr = false;
	}

	if (attachStdIn || attachStdout || attachStdErr) {
		// If not directed to a file, try attaching to console,
		// or if repl, create a new one
		if (AttachConsole(ATTACH_PARENT_PROCESS)) {
			attachedToConsole = true;
		} else if (repl && AllocConsole()) {
			attachedToConsole = true;
		}

		if (attachedToConsole) {
			if (attachStdIn) {
				freopen("CONIN$", "r", stdin);
				setvbuf(stdin, nullptr, _IONBF, 0);
			}
			if (attachStdout) {
				freopen("CONOUT$", "w", stdout);
				setvbuf(stdout, nullptr, _IONBF, 0);
			}
			if (attachStdErr) {
				freopen("CONOUT$", "w", stderr);
				setvbuf(stderr, nullptr, _IONBF, 0);
			}
		}
	}
}
