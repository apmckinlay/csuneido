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
	RunPiped(char* cmd);
	void write(char* buf, int len);
	void write(char* buf)
		{ write(buf, strlen(buf)); }
	int read(char* buf, int len);
	void closewrite();
	~RunPiped();
private:
	HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
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
		except("RunPiped: CreateProcess failed");
	
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	CloseHandle(hChildStdinRd);
	CloseHandle(hChildStdoutWr);
	}

void RunPiped::write(char* buf, int len)
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

void RunPiped::closewrite()
	{
	CloseHandle(hChildStdinWr);
	hChildStdinWr = 0;
	}

RunPiped::~RunPiped()
	{
	if (hChildStdinWr)
		CloseHandle(hChildStdinWr);
	CloseHandle(hChildStdoutRd);
	}

#ifndef TEST
#include "builtinclass.h"
#include "sufinalize.h"
#include "sustring.h"

class SuRunPiped : public SuFinalize
	{
public:
	void init(const gcstring& cmd)
		{
		rp = new RunPiped(strdup(cmd.str()));
		}
	virtual void out(Ostream& os)
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
			Method<SuRunPiped>("CloseWrite", &SuRunPiped::CloseWrite),
			Method<SuRunPiped>("Close", &SuRunPiped::Close),
			Method<SuRunPiped>("", 0)
			};
		return methods;
		}
	const char* type() const
		{ return "RunPiped"; }
	void close();
private:
	Value Read(BuiltinArgs&);
	Value Readline(BuiltinArgs&);
	Value Write(BuiltinArgs&);
	Value CloseWrite(BuiltinArgs&);
	Value Close(BuiltinArgs&);

	void ckopen();
	virtual void finalize();
	RunPiped* rp;
	gcstring cmd;
	};

Value su_runpiped()
	{
	static BuiltinClass<SuRunPiped> suRunPipedClass;
	return &suRunPipedClass;
	}

void BuiltinClass<SuRunPiped>::out(Ostream& os)
	{ 
	os << "RunPiped /* builtin class */";
	}

Value BuiltinClass<SuRunPiped>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: RunPiped(command)");
	gcstring cmd = args.getgcstr("command");
	args.end();
	SuRunPiped* runpiped = new BuiltinInstance<SuRunPiped>();
	runpiped->init(cmd);
	return runpiped;
	}

template<>
Value BuiltinClass<SuRunPiped>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: RunPiped(filename, mode = 'r', block = false)");
	gcstring cmd = args.getgcstr("command");
	Value block = args.getValue("block", SuFalse);
	args.end();

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

Value SuRunPiped::Readline(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Readline()");
	args.end();

	ckopen();
	bool data = false;
	char c;
	std::vector<char> buf;
	while (0 != rp->read(&c, 1))
		{
		data = true;
		if (c == '\n')
			break ;
		if (c == '\r')
			{
			rp->read(&c, 1);
			break ;
			}
		buf.push_back(c);
		}
	if (! data)
		return SuFalse;
	buf.push_back(0); // allow space for nul
	return new SuString(buf.size() - 1, &buf[0]); // no alloc
	}

Value SuRunPiped::Write(BuiltinArgs& args)
	{
	args.usage("usage: runpiped.Write(s)");
	gcstring s = args.getgcstr("s");
	args.end();
	
	ckopen();
	rp->write(s.buf(), s.size());
	return Value();
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
