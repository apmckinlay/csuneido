#ifndef TEMPDB_H
#define TEMPDB_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2000 Suneido Software Corp. 
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

#include <stdio.h>	// for remove
#include "database.h"

extern Database* thedb;

class TempDB // need class to ensure thedb is restored
	{
public:
	TempDB(bool r = true) : rm(r)
		{
		remove("tempdb");
		savedb = thedb; 
		thedb = new Database("tempdb", DBCREATE);
		//~ for (int i = 0; i < 1100; ++i)
			//~ thedb->mmf->alloc(4000000, MM_OTHER, false);
		}
	void reopen()
		{
		delete thedb;
		thedb = new Database("tempdb");
		}
	void close()
		{
		delete thedb;
		thedb = savedb;
		}
	void open()
		{
		thedb = new Database("tempdb");
		}
	~TempDB()
		{
		close();
		if (rm)
			remove("tempdb");
		}
private:
	Database* savedb;
	bool rm;
	};

#endif
