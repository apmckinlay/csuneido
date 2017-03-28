#pragma once
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

// allocate "permanent" (never freed) memory
// NOT garbage collected
// should NOT contain reference to garbage collected memory

class PermanentHeap
	{
public:
	PermanentHeap(const char* name, int size);
	void* alloc(int size);	// grows heap
	void free(void* p);		// shrinks heap
	bool contains(const void* p) const
		{ return start <= p && p < next; }
	bool inarea(const void* p) const
		{ return start <= p && p < limit; }
	void* begin() const
		{ return start; }
	void* end() const
		{ return next; }
	int size() const
		{ return next - (char*) start; }
	int remaining() const
		{ return (char*) limit - next; }
private:
	const char* name;
	void* start;
	char* next;
	char* commit_end;
	void* limit;
	};
