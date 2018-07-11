#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "lisp.h"
#include "gcstring.h"
#include "value.h"

class Row;
class Header;

typedef Lisp<gcstring> Fields;

class Expr
	{
public:
	virtual ~Expr() = default;
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
		{ return SuFalse; }
	virtual Expr* rename(const Fields& from, const Fields& to) = 0;
	virtual Expr* replace(const Fields& from, const Lisp<Expr*>& to) = 0;
	virtual Expr* fold() = 0;
	};

inline Ostream& operator<<(Ostream& os, const Expr& x)
	{ x.out(os); return os; }
inline Ostream& operator<<(Ostream& os, Expr* x)
	{ return os << *x; }
