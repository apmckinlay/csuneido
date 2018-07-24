// Copyright (c) 2004 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "testing.h"
#include "testobstd.h"
#include "ostreamstr.h"
#include "malloc.h"
#include "except.h"
#include "ostreamstd.h"
#include <signal.h>
#include "cmdlineoptions.h"

bool is_server = false;
CmdLineOptions cmdlineoptions;

static void sigill(int signo) {
	except("illegal instruction");
}

static void sigsegv(int signo) {
	except("memory fault");
}

int main(int argc, char** argv) {
	signal(SIGILL, sigill);
	signal(SIGSEGV, sigsegv);

	TestObserverStd to;
	if (argc == 1)
		TestRegister::runall(to);
	for (--argc, ++argv; argc > 0; --argc, ++argv)
		if (-1 == TestRegister::runtest(*argv, to))
			cout << "can't find " << *argv << endl;
	return 0;
}

Except::Except(char* x) : exception(x) {
}

Ostream& operator<<(Ostream& os, const Except& x) {
	return os << x.exception;
}

static OstreamStr os(200);

Ostream& osexcept() {
	return os;
}

void except_() {
	Except x(os.str());
	os.clear();
	throw x;
}
