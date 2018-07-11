// Copyright (c) 2007 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// see: http://msdn2.microsoft.com/en-us/library/ms682499.aspx

#include "win.h"
#include <windows.h>

#ifndef TEST
#include "except.h"
#else
#define except(s) \
	do { MessageBox(0, "RunPiped", s, 0); \
	ExitProcess(0); } while (false)
#endif

int RunPiped_count = 0;

class RunPiped
	{
public:
	explicit RunPiped(char* cmd);
	void write(const char* buf, int len);
	void write(char* buf)
		{ write(buf, strlen(buf)); }
	int read(char* buf, int len);
	void flush();
	void closewrite();
	void close();
	int exitvalue();
	~RunPiped();
private:
	HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
	HANDLE hProcess;
	};

RunPiped::RunPiped(char* cmd)
	{
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
		except("RunPiped: stdout pipe creation failed");

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

	if (! CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0))
		except("RunPiped: stdin pipe creation failed");

	// Ensure the write handle to the pipe for STDIN is not inherited.
	SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

	PROCESS_INFORMATION piProcInfo;
	ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

	STARTUPINFO siStartInfo;
	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = hChildStdoutWr;
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	BOOL bFuncRetn = CreateProcess(
		NULL, 		// application name (NULL = use command line)
		cmd, 			// command line
		NULL,			// process security attributes
		NULL,			// primary thread security attributes
		TRUE,		// handles are inherited
		CREATE_NO_WINDOW,			// creation flags
		NULL,			// use parent's environment
		NULL,			// use parent's current directory
		&siStartInfo,	// STARTUPINFO pointer
		&piProcInfo);	// receives PROCESS_INFORMATION

	if (bFuncRetn == 0)
		except("RunPiped: CreateProcess failed for: " << cmd);

	hProcess = piProcInfo.hProcess;
	CloseHandle(piProcInfo.hThread);

	CloseHandle(hChildStdinRd);
	CloseHandle(hChildStdoutWr);
	++RunPiped_count;
	}

void RunPiped::write(const char* buf, int len)
	{
	DWORD dwWritten;

	if (! WriteFile(hChildStdinWr, buf, len, &dwWritten, NULL) ||
		dwWritten != len)
		except("RunPiped: write failed");
	}

int RunPiped::read(char* buf, int len)
	{
	DWORD dwRead;

	if (! ReadFile(hChildStdoutRd, buf, len, &dwRead, NULL))
		return 0;
	return dwRead;
	}

void RunPiped::flush()
	{
	FlushFileBuffers(hChildStdinWr);
	}

void RunPiped::closewrite()
	{
	CloseHandle(hChildStdinWr);
	hChildStdinWr = 0;
	}

void RunPiped::close()
	{
	if (! hChildStdoutRd)
		return;
	if (hChildStdinWr)
		closewrite();
	CloseHandle(hChildStdoutRd);
	hChildStdoutRd = 0;
	--RunPiped_count;
	}

RunPiped::~RunPiped()
	{
	close();
	CloseHandle(hProcess);
	}

int RunPiped::exitvalue()
	{
	close();
	const int ONE_MINUTE = 60 * 1000;
	if (0 != WaitForSingleObject(hProcess, ONE_MINUTE))
		except("RunPiped: exitvalue wait failed");
	DWORD exitcode;
	if (! GetExitCodeProcess(hProcess, &exitcode))
		except("RunPiped: exitvalue get exit code failed");
	CloseHandle(hProcess);
	return exitcode;
	}

#ifndef TEST
#include "builtinclass.h"
#include "sustring.h"
#include "ostreamstr.h"
#include "readline.h"

class SuRunPiped : public SuValue
	{
public:
	void init(const gcstring& c)
		{
		cmd = c;
		rp = new RunPiped(dupstr(cmd.str()));
		}

	static auto methods()
		{
		static Method<SuRunPiped> methods[]
			{
			{ "Read", &SuRunPiped::Read },
			{ "Readline", &SuRunPiped::Readline },
			{ "Write", &SuRunPiped::Write },
			{ "Writeline", &SuRunPiped::Writeline },
			{ "Flush", &SuRunPiped::Flush },
			{ "CloseWrite", &SuRunPiped::CloseWrite },
			{ "Close", &SuRunPiped::Close },
			{ "ExitValue", &SuRunPiped::ExitValue }
			};
		return gsl::make_span(methods);
		}
	void close();
private:
	Value Read(BuiltinArgs&);
	Value Readline(BuiltinArgs&);
	Value Write(BuiltinArgs&);
	Value Writeline(BuiltinArgs&);
	Value Flush(BuiltinArgs&);
	Value CloseWrite(BuiltinArgs&);
	Value Close(BuiltinArgs&);
	Value ExitValue(BuiltinArgs&);

	void ckopen();
	void write(BuiltinArgs&);
	RunPiped* rp = nullptr;
	gcstring cmd;
	};

Value su_runpiped()
	{
	static BuiltinClass<SuRunPiped> suRunPipedClass("(command, block = false)");
	return &suRunPipedClass;
	}

template<>
Value BuiltinClass<SuRunPiped>::instantiate(BuiltinArgs& args)
	{
	args.usage("RunPiped(command)");
	gcstring cmd = args.getgcstr("command");
	SuRunPiped* runpiped = new BuiltinInstance<SuRunPiped>();
	runpiped->init(cmd);
	return runpiped;
	}

template<>
Value BuiltinClass<SuRunPiped>::callclass(BuiltinArgs& args)
	{
	args.usage("RunPiped(command, block = false)");
	gcstring cmd = args.getgcstr("command");
	Value block = args.getValue("block", SuFalse);
	SuRunPiped* rp = new BuiltinInstance<SuRunPiped>();
	rp->init(cmd);
	if (block == SuFalse)
		return rp;
	Closer<SuRunPiped*> closer(rp);
	KEEPSP
	PUSH(rp);
	return block.call(block, CALL, 1);
	}

Value SuRunPiped::Read(BuiltinArgs& args)
	{
	args.usage("runpiped.Read(n = 1024)");
	int n = args.getint("n", 1024);
	args.end();

	ckopen();
	char* buf = salloc(n);
	n = rp->read(buf, n);
	return n == 0 ? SuFalse : SuString::noalloc(buf, n);
	}

// NOTE: Readline should be consistent across file, socket, and runpiped
Value SuRunPiped::Readline(BuiltinArgs& args)
	{
	args.usage("runpiped.Readline()").end();

	ckopen();
	char c;
	READLINE(0 != rp->read(&c, 1));
	}

Value SuRunPiped::Write(BuiltinArgs& args)
	{
	args.usage("runpiped.Write(s)");
	write(args);
	return Value();
	}

Value SuRunPiped::Writeline(BuiltinArgs& args)
	{
	args.usage("runpiped.Writeline(s)");
	write(args);
	rp->write("\r\n", 2);
	return Value();
	}

Value SuRunPiped::Flush(BuiltinArgs& args)
	{
	args.usage("runpiped.Flush()").end();
	rp->flush();
	return Value();
	}

void SuRunPiped::write(BuiltinArgs& args)
	{
	gcstring s = args.getValue("s").to_gcstr();
	args.end();

	ckopen();
	rp->write(s.ptr(), s.size());
	}

Value SuRunPiped::CloseWrite(BuiltinArgs& args)
	{
	args.usage("runpiped.CloseWrite()").end();

	ckopen();
	rp->closewrite();
	return Value();
	}

Value SuRunPiped::Close(BuiltinArgs& args)
	{
	args.usage("runpiped.Close()").end();

	ckopen();
	close();
	return Value();
	}

Value SuRunPiped::ExitValue(BuiltinArgs& args)
	{
	args.usage("runpiped.ExitValue()").end();
	return rp->exitvalue();
	}

void SuRunPiped::ckopen()
	{
	if (! rp)
		except("RunPiped: already closed");
	}

void SuRunPiped::close()
	{
	if (rp)
		delete rp;
	rp = nullptr;
	}

#else // TEST

#include <stdio.h>

int main(int, char**)
	{
	RunPiped rp("ed");
	char buf[1024];

	rp.write("a\n");
	rp.write("hello world\n");
	rp.write(".\n");
	rp.write("1,$p\n");
	rp.write("Q\n");
	while (int n = rp.read(buf, sizeof (buf)))
		fwrite(buf, n, 1, stdout);
	}
#endif
