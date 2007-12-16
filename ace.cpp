#include "cmdlineoptions.h"
#include "aceserver.h"
#include "errlog.h"
#include "ostreamstd.h"
#include "except.h"
#include "fatal.h"
#include <stdlib.h> // for exit
#include <exception>
#include "gc-7.0/include/gc.h"
#include "interp.h" // for proc

void builtins();

bool is_server = true;
bool is_client = false;
char* cmdline = "";
CmdLineOptions cmdlineoptions;
char* fiber_id = "";
char* session_id = "";

const int N_THREADS = 2; // for initial development

int main(int argc, char**argv)
	{
	GC_init();
	try
		{
		proc = new Proc;
		builtins(); // internal initialization
		aceserver(N_THREADS);
		}
	catch (const Except& x)
		{
		fatal(x.exception);
		}
	catch (const std::exception& e)
		{
		fatal(e.what());
		}
	catch (...)
		{
		fatal("unknown exception");
		}
	}

void alertout(char* s)
	{
	errlog("Alert: ", s);
	}

void ckinterrupt()
	{
	}

void fatal(const char* error, const char* extra)
	{
	cout << "fatal error: " << error << " " << extra << endl;
	errlog("fatal error:", error, extra);
	exit(-1);
	}
