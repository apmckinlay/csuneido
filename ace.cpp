#include "cmdlineoptions.h"
#include "aceserver.h"
#include "errlog.h"

bool is_server = true;
bool is_client = false;
char* cmdline = "";
CmdLineOptions cmdlineoptions;
char* fiber_id = "";
char* session_id = "";

const int N_THREADS = 1; // for initial development

int main(int argc, char**argv)
	{
	aceserver(N_THREADS);
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
	errlog("fatal error:", error, extra);
	}
