// Copyright (c) 2006 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "circlog.h"
#include "gcstring.h"
#include "value.h"
#include "interp.h"
#include "builtin.h"
#include "sustring.h"
#include "ostreamstr.h"
#include "buffer.h"
#include <algorithm>

const int QSIZE = 500;
static gcstring queue[QSIZE];
static int qi = 0;

void circ_log(const gcstring& s)
	{
	queue[qi] = s.trim();
	qi = (qi + 1) % QSIZE;
	}

gcstring circ_log_get()
	{
	Buffer buf;
	int i = qi;
	do
		{
		if (queue[i] != "") // uninitialized
			buf.add(queue[i]).add('\n');
		i = (i + 1) % QSIZE;
		} while (i != qi);
	return buf.gcstr();
	}

BUILTIN(CircLog, "(string=false)")
	{
	int nargs = 1;
	if (ARG(0) == SuFalse)
		return new SuString(circ_log_get());
	circ_log(ARG(0).gcstr());
	return Value();
	}

static OstreamStr os(500);

Ostream& oscirclog()
	{
	return os;
	}

void circlogos_()
	{
	circ_log(os.gcstr());
	os.clear();
	}

// tests ------------------------------------------------------------

#include "testing.h"

static void clear()
	{
	qi = 0;
	std::fill(queue, queue + QSIZE, "");
	}

TEST(circlog)
	{
	clear();
	assert_eq(circ_log_get(), "");
	circ_log("one");
	assert_eq(circ_log_get(), "one\n");
	circ_log("two");
	assert_eq(circ_log_get(), "one\ntwo\n");
	}
