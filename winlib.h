#ifndef WINLIB_H
#define WINLIB_H

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

// handle LoadLibrary and FreeLibrary for dll's
struct WinLib
	{
	WinLib() : lib(0)
		{ }
	explicit WinLib(char* s) : name(dupstr(s))
		{ lib = LoadLibrary(name); }
	~WinLib()
		{
		if (lib)
			FreeLibrary(lib); 
		}
	void* GetProcAddress(char* name)
		{ return (void*) ::GetProcAddress(lib, name); }
	void retarget(char* name)
		{
		if (lib)
			FreeLibrary(lib);
		lib = LoadLibrary(name);
		}
	operator void*()
		{ return lib; }

	char* name;
	HMODULE lib;
	};

#endif
