/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2004 Suneido Software Corp. 
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

#include "sesviews.h"
#include "hashmap.h"
#include "tls.h"

static HashMap<gcstring,gcstring>* session_views = 0;
TLS(session_views);

void set_session_view(const gcstring& name, const gcstring& def)
	{
	if (! session_views)
		session_views = new HashMap<gcstring,gcstring>;
	(*session_views)[name] = def;
	}

gcstring get_session_view(const gcstring& name)
	{
	if (session_views)
		if (gcstring* p = session_views->find(name))
			return *p;
	return "";
	}

void remove_session_view(const gcstring& name)
	{
	if (session_views)
		session_views->erase(name);
	}

void new_session(void** p)
	{
	if (! *p)
		*p = new HashMap<gcstring,gcstring>;
	session_views = (HashMap<gcstring,gcstring>*) *p;
	}
