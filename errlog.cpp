// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "errlog.h"
#include "ostreamfile.h"
#include "fibers.h" // for tls()
#include "unhandled.h" // for err_filename()
#include <ctime>
#include "fatal.h"

extern bool is_client;

static int count = 0;
const int LIMIT = 100;

void errlog(const char* msg1, const char* msg2, const char* msg3)
	{
	if (++count > LIMIT)
		fatal("too many errors, exiting");
	errlog_uncounted(msg1, msg2, msg3);
	}

void errlog_uncounted(const char* msg1, const char* msg2, const char* msg3)
	{
	OstreamFile log(is_client ? err_filename() : "error.log", "at");
	time_t t;
	time(&t);
	char buf[100];
	strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&t));
	log << buf << ' ';
	if (*tls().fiber_id)
		log << tls().fiber_id << ": ";
	log << msg1 << " " << msg2 << " " << msg3 << endl;
	}
