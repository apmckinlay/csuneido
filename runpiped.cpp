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
		0,			// creation flags 
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
	
	if (!ReadFile(hChildStdoutRd, buf, len, &dwRead, NULL))
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
	try
		{
		Value* sp = proc->stack.getsp();
		proc->stack.push(rp);
		Value result = block.call(block, CALL, 1, 0, 0, -1);
		proc->stack.setsp(sp);
		rp->close();
		return result;
		}
	catch(...)
		{
		rp->close();
		throw ;
		}
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
