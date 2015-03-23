#ifndef BUFFER_H
#define BUFFER_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2013 Suneido Software Corp. 
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

class gcstring;

class Buffer
	{
public:
	explicit Buffer(int n = 128);

	// make sure the buffer has space for n more characters
	// afterwards, available() will be >= n
	// NOTE: should be followed by calling added()
	char* ensure(int n);

	// does ensure(n) and then added(n)
	char* alloc(int n);

	Buffer& add(char c);
	Buffer& add(const char* s, int n);
	Buffer& add(const gcstring& s);

	void remove(int n);
	int size()
		{ return used; }
	char* buffer()
		{ return buf; }
	void added(int n)
		{ used += n; }
	void clear()
		{ used = 0; }
	int available()
		{ return capacity - used; }
	char* bufnext()
		{ return buf + used; }
	char* str(); // no alloc
	gcstring gcstr(); // no alloc
private:
	int capacity;
	char* buf;
	int used;
	};

#endif