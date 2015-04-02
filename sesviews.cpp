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
#include "fibers.h" // for tls()

void set_session_view(const gcstring& name, const gcstring& def)
	{
	if (! tls().session_views)
		tls().session_views = new SesViews;
	(*tls().session_views)[name] = def;
	}

gcstring get_session_view(const gcstring& name)
	{
	if (tls().session_views)
		if (gcstring* p = tls().session_views->find(name))
			return *p;
	return "";
	}

void remove_session_view(const gcstring& name)
	{
	if (tls().session_views)
		tls().session_views->erase(name);
	}
