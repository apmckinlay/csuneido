/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2002 Suneido Software Corp. 
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

#include "suseq.h"
#include "suobject.h"
#include "interp.h"

SuSeq::SuSeq(Value i) : iter(i), ob(0)
	{ }

void SuSeq::out(Ostream& os)
	{ 
	build();
	ob->out(os);
	}

Value SuSeq::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value ITER("Iter");
	static Value NEXT("Next");
	static Value COPY("Copy");
	static Value REWIND("Rewind");

	if (member == ITER)
		{
		if (ob)
			return ob->call(self, member, nargs, nargnames, argnames, each);
		Value newiter = iter.call(iter, COPY, 0, 0, 0, -1);
		newiter.call(newiter, REWIND, 0, 0, 0, -1);
		return newiter;
		}
	else if (member == NEXT)
		{
		return iter.call(iter, NEXT, 0, 0, 0, -1);
		}
	else if (member == COPY && ! ob)
		{
		// avoid two copies (build & copy)
		// for common usage: for m in ob.Members().Copy()
		return copy();
		}
	else
		{
		if (SuObject* x = val_cast<SuObject*>(iter))
			if (x->hasMethod(member))
				return iter.call(iter, member, nargs, nargnames, argnames, each);
		build();
		return ob->call(self, member, nargs, nargnames, argnames, each);
		}
	}

void SuSeq::build() const
	{
	if (ob)
		return ;
	ob = copy();
	}

SuObject* SuSeq::copy() const
	{
	static Value REWIND("Rewind");

	SuObject* copy = new SuObject;
	iter.call(iter, REWIND, 0, 0, 0, -1);
	Value x;
	while (iter != (x = next()))
		copy->add(x);
	return copy;
	}

Value SuSeq::next() const
	{
	static Value NEXT("Next");

	KEEPSP
	return iter.call(iter, NEXT, 0, 0, 0, -1);
	}

void SuSeq::putdata(Value i, Value x)
	{
	build();
	return ob->putdata(i, x);
	}

Value SuSeq::getdata(Value i)
	{
	build();
	return ob->getdata(i);
	}

size_t SuSeq::packsize() const
	{
	build();
	return ob->packsize();
	}

void SuSeq::pack(char* buf) const
	{
	build();
	ob->pack(buf);
	}

bool SuSeq::lt(const SuValue& y) const
	{
	build();
	return ob->lt(y);
	}

bool SuSeq::eq(const SuValue& y) const
	{
	build();
	return ob->eq(y);
	}

SuObject* SuSeq::object() const
	{
	build();
	return ob;
	}

SuObject* SuSeq::ob_if_ob()
	{
	build();
	return ob;
	}

// SuSeqIter ========================================================

SuSeqIter::SuSeqIter(Value f, Value t, Value b) : from(f), to(t), by(b), i(f)
	{
	if (to == SuFalse)
		{
		to = from;
		i = from = 0;
		}
	}

void SuSeqIter::out(Ostream& os)
	{
	os << "SeqIter";
	}


Value SuSeqIter::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value ITER("Iter");
	static Value COPY("Copy");
	static Value NEXT("Next");
	static Value REWIND("Rewind");

	if (member == ITER)
		{
		return this;
		}
	else if (member == NEXT)
		{
		// TODO: handle negative by
		if (i >= to)
			return this;
		Value val = i;
		i = i + by;
		return val;
		}
	else if (member == COPY)
		{
		return new SuSeqIter(from, to, by);
		}
	else if (member == REWIND)
		{
		i = from;
		return Value();
		}
	else
		method_not_found(type(), member);
	}

#include "interp.h"
#include "prim.h"

Value su_seq()
	{
	const int nargs = 3;
	return new SuSeq(new SuSeqIter(ARG(0), ARG(1), ARG(2)));
	}
PRIM(su_seq, "Seq(from, to = false, by = 1)");

// Sequence =========================================================

Value su_sequence()
	{
	const int nargs = 1;
	if (SuObject* ob = val_cast<SuObject*>(ARG(0)))
		return new SuSeq(ob);
	else
		except("usage: Sequence(object)");
	}
PRIM(su_sequence, "Sequence(object)");
