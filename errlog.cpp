/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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

#include "errlog.h"
#include "ostreamfile.h"
#include "fibers.h" // for tls()
#include "unhandled.h" // for err_filename()
#include <time.h>
#include "fatal.h"

extern bool is_client;

static int count = 0;
const int LIMIT = 100;

void errlog(const char* msg1, const char* msg2, const char* msg3)
	{
	if (++count > LIMIT)
		fatal("too many errors");
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
