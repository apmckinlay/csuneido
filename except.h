#ifndef EXCEPT_H
#define EXCEPT_H

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

#include "ostream.h"
#include "std.h" // for NORETURN

NORETURN(except_());
NORETURN(except_err_());

Ostream& osexcept();

#define except(msg) \
	((osexcept() << msg), except_())

#define except_err(msg) \
	((osexcept() << msg), except_err_())

#define except_if(e, msg) \
	((e) ? except(msg) : (void) 0)

#define error(msg) \
	except_err(__FILE__ << ':' << __LINE__ << ": " << msg)

#define verify(e) \
	((e) ? (void) 0 : error("assertion failure: " << #e))

#define asserteq(x, y) \
	((x) == (y) ? (void) 0 : error("assertion failure: " << \
	"\n" << #x << " " << (x) << "\n!=\n" << #y << " " << (y)))

#define unreachable()	error("should not reach here!") 
#define unimplemented()	error("not implemented yet") 

class Frame;
class Value;

struct Except
	{
	explicit Except(char* s);
	char* exception;
	Frame* fp;
	Value* sp;
	};

Ostream& operator<<(Ostream& os, const Except& x);

#endif