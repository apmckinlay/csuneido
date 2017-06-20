#pragma once
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

/// Used by circlog, ostreamstr, sockets, sustring
class Buffer
	{
public:
	explicit Buffer(int n = 128);

	/// Ensure there is space for at least n more bytes and update used.
	/// @return A pointer into the buffer at the start of the space.
	char* alloc(int n);

	/// Make sure there is space for at least n more bytes.
	/// Allocates a larger buffer if required.
	/// NOTE: This does not advance used,
	/// it should be followed by calling added()
	/// @post available() >= n
	/// @return A pointer into the buffer where data should be added.
	char* ensure(int n);

	/// Call after adding data directly into the buffer. Updates used.
	void added(int n)
		{ used += n; }

	/// Adds a single char to the buffer, updating used.
	Buffer& add(char c);

	/// Copies data into the buffer, updating used.
	Buffer& add(const char* s, int n);

	/// Copies data into the buffer, updating used.
	Buffer& add(const gcstring& s);

	/// Remove data from buffer by moving remaining data.
	/// WARNING: Invalidates previous references.
	void remove(int n);

	/// @return the number of bytes currently in the buffer.
	int size() const
		{ return used; }

	/// @return A pointer to the entire buffer.
	char* buffer() const
		{ return buf; }

	/// @return The unused space left in the buffer (capacity - used)
	int available() const
		{ return capacity - used; }

	/// @return The number of bytes left to read (used - pos)
	int remaining() const
		{ return used - pos; }

	/// @return A pointer to the entire buffer, with a nul added at the end.
	/// References the buffer, does not copy.
	char* str() const;

	/// @return The used portion of the buffer.
	/// References the buffer, does not copy.
	gcstring gcstr() const;

	/// @return The next byte at pos. Advances pos, reducing remaining()
	char get()
		{ return buf[pos++]; }

	/// @return A copy of n bytes as a gcstring.
	/// Advances pos by n, reducing remaining()
	gcstring getStr(int n);

	/// @return A pointer into the buffer. Does *not* copy the data.
	/// Advances pos by n, reducing remaining()
	char* getBuf(int n);

	/// Resets used and pos to 0. Does not alter size of buffer.
	Buffer& clear()
		{
		used = pos = 0;
		return *this;
		}

private:
	char* buf;
	int capacity; ///< Size of buf
	int used;     ///< Where to add more at, and the limit for reading
	int pos;      ///< The position for reading
	};
