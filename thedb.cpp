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

#include "thedb.h"
#include "database.h"
#include "port.h"

bool thedb_create = false;
Database* thedb = 0;

void close_db()
	{
	delete thedb;
	thedb = 0;
	}

struct CloseDB
	{
	~CloseDB()
		{ close_db(); }
	};
static CloseDB closeDB; // static so destroyed on exit

Database* theDB()
	{
	if (! thedb)
		{
		thedb = new Database("suneido.db", thedb_create);
		sync_timer(thedb->mmf); 
		}
	return thedb;
	}
