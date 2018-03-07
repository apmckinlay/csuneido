/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2006 Suneido Software Corp.
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

#include "circlog.h"
#include "gcstring.h"
#include "value.h"
#include "interp.h"
#include "prim.h"
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

Value su_circ_log()
	{
	int nargs = 1;
	if (ARG(0) == SuFalse)
		return new SuString(circ_log_get());
	circ_log(ARG(0).gcstr());
	return Value();
	}
PRIM(su_circ_log, "CircLog(string=false)");

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

#include "testing.h"

static void clear()
	{
	qi = 0;
	std::fill(queue, queue + QSIZE, "");
	}

class test_circlog : public Tests
	{
	TEST(0, "main")
		{
		clear();
		assert_eq(circ_log_get(), "");
		circ_log("one");
		assert_eq(circ_log_get(), "one\n");
		circ_log("two");
		assert_eq(circ_log_get(), "one\ntwo\n");
		}
	};
REGISTER(test_circlog);
