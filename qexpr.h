#ifndef QEXPR_H
#define QEXPR_H

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

#include "lisp.h"
#include "gcstring.h"
#include "suboolean.h"
#include "value.h"

class Row;
class Header;

typedef Lisp<gcstring> Fields;

class Expr
	{
public:
	virtual Fields fields() = 0;
	virtual void out(Ostream& os) const = 0;
	// used for optimizing select expressions
	virtual bool term(const Fields& fields)
		{ return false; } // overridden by BinOp and In
	virtual bool is_term(const Fields& fields)
		{ return false; } // overridden by BinOp and In
	virtual bool isfield(const Fields& fields)
		{ return false; }
	// for select execution
	virtual Value eval(const Header&, const Row&)
		{ return SuBoolean::f; }
	virtual Expr* rename(const Fields& from, const Fields& to) = 0;
	virtual Expr* replace(const Fields& from, const Lisp<Expr*>& to) = 0;
	virtual Expr* fold() = 0;
	};

inline Ostream& operator<<(Ostream& os, const Expr& x)
	{ x.out(os); return os; }
inline Ostream& operator<<(Ostream& os, Expr* x)
	{ return os << *x; }

#endif
