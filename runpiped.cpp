/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2007 Suneido Software Corp.
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
#include "sufinalize.h"
#include "sustring.h"
#include "ostreamstr.h"
#include "readline.h"

class SuRunPiped : public SuFinalize
	{
public:
	void init(const gcstring& c)
		{
		cmd = c;
		rp = new RunPiped(dupstr(cmd.str()));
		}

	void out(Ostream& os) const override
		{
		os << "RunPiped('" << cmd << "')";
		}
	static Method<SuRunPiped>* methods()
		{
		static Method<SuRunPiped> methods[] =
			{
			Method<SuRunPiped>("Read", &SuRunPiped::Read),
			Method<SuRunPiped>("Readline", &SuRunPiped::Readline),
			Method<SuRunPiped>("Write", &SuRunPiped::Write),
			Method<SuRunPiped>("Writeline", &SuRunPiped::Writeline),
			Method<SuRunPiped>("Flush", &SuRunPiped::Flush),
			Method<SuRunPiped>("CloseWrite", &SuRunPiped::CloseWrite),
			Method<SuRunPiped>("Close", &SuRunPiped::Close),
			Method<SuRunPiped>("ExitValue", &SuRunPiped::ExitValue),
			Method<SuRunPiped>("", 0)
			};
		return methods;
		}
	const char* type() const override
		{ return "RunPiped"; }
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
	void finalize() override;
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
void BuiltinClass<SuRunPiped>::out(Ostream& os) const
	{
	os << "RunPiped /* builtin class */";
	}

template<>
Value BuiltinClass<SuRunPiped>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: RunPiped(command)");
	gcstring cmd = args.getgcstr("command");
	SuRunPiped* runpiped = new BuiltinInstance<SuRunPiped>();
	runpiped->init(cmd);
	return runpiped;
	}

template<>
Value BuiltinClass<SuRunPiped>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: RunPiped(command, block = false)");
	gcstring cmd = args.getgcstr("command");
	Value block = args.getValue("block", SuFalse);
	SuRunPiped* rp = new BuiltinInstance<SuRunPiped>();
	rp->init(cmd);
	if (block == SuFalse)
		return rp;
	Closer<SuRunPiped*> closer(rp);
	KEEPSP
	PUSH(rp);
	return block.call(block, CALL, 1, 0, 0, -1);
	}

Value SuRunPiped::Read(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Read(n = 1024)");
	int n = args.getint("n", 1024);
	args.end();

	ckopen();
	gcstring gcstr(n);
	n = rp->read(gcstr.buf(), n);

	return n == 0 ? SuFalse : new SuString(gcstr.substr(0, n));
	}

// NOTE: Readline should be consistent across file, socket, and runpiped
Value SuRunPiped::Readline(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Readline()");
	args.end();

	ckopen();
	char c;
	READLINE(0 != rp->read(&c, 1));
	}

Value SuRunPiped::Write(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Write(s)");
	write(args);
	return Value();
	}

Value SuRunPiped::Writeline(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Writeline(s)");
	write(args);
	rp->write("\r\n", 2);
	return Value();
	}

Value SuRunPiped::Flush(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Flush()");
	args.end();
	rp->flush();
	return Value();
	}

void SuRunPiped::write(BuiltinArgs& args)
	{
	gcstring s = args.getgcstr("s");
	args.end();

	ckopen();
	rp->write(s.buf(), s.size());
	}

Value SuRunPiped::CloseWrite(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.CloseWrite()");
	args.end();

	ckopen();
	rp->closewrite();
	return Value();
	}

Value SuRunPiped::Close(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Close()");
	args.end();

	ckopen();
	close();
	return Value();
	}

Value SuRunPiped::ExitValue(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.ExitValue()");
	args.end();
	return rp->exitvalue();
	}

void SuRunPiped::ckopen()
	{
	if (! rp)
		except("RunPiped: already closed");
	}

void SuRunPiped::close()
	{
	removefinal();
	finalize();
	}

void SuRunPiped::finalize()
	{
	if (rp)
		delete rp;
	rp = 0;
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
