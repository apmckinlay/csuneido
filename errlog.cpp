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
#include <string.h>
#include "ostreamfile.h"
#include "fibers.h" // for tss_fiber_id()
#include <time.h>

extern bool is_client;
extern char* err_filename();

void errlog(const char* msg1, const char* msg2, const char* msg3)
	{
	OstreamFile log(is_client ? err_filename() : "error.log", "at");
	time_t t;
	time(&t);
	char* s = ctime(&t);
	if (! s)
		s = "???";
	else
		s[strlen(s) - 1] = 0;
	log << s << ' ';
	if (*tss_fiber_id())
		log << tss_fiber_id() << ": ";
	log << msg1 << " " << msg2 << " " << msg3 << endl;
	}
