#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2018 Suneido Software Corp.
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

class Ostream;
template <typename T> class List; 

using PTfn = bool (*)(const List<const char*>& args, Ostream& os);

struct PortTest
	{
	PortTest(const char* name, PTfn fn);
	static bool run_file(const char* filename, Ostream& os);

	const char* name;
	PTfn fn;
	};

#define PORTTEST(name) \
	static bool name(const List<const char*>& args, Ostream& os); \
	PortTest pt_##name(#name, name); \
	static bool name(const List<const char*>& args, Ostream& os)
