#ifndef ISTREAM_H
#define ISTREAM_H

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

// abstract base class for input streams
class Istream
	{
public:
	Istream();
	virtual ~Istream()
		{ }
	enum { EOFVAL = -1 };
	int get();
	Istream& getline(char* buf, int n);
	bool eof()
		{ return eofbit; }
	explicit operator bool() const
		{ return !failbit; }
	void putback(char c)
		{ next = c; }
	int peek()
		{ return next = get(); }
	void clear()
		{ eofbit = failbit = false; }
	Istream& read(char* buf, int n);
	int gcount()
		{ return gcnt; }
	virtual int tellg() = 0;
	virtual Istream& seekg(int pos) = 0;
protected:
	virtual int get_() = 0;
	virtual int read_(char* buf, int n) = 0;
private:
	bool eofbit;
	bool failbit;
	int next;
	int gcnt;
	};

#endif
