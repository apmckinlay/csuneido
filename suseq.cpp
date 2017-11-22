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
#include "globals.h"
#include "sustring.h"
#include "ostreamstr.h"

SuSeq::SuSeq(Value i) : iter(i)
	{ }

void SuSeq::out(Ostream& os) const
	{
	if (infinite(iter))
		os << "infiniteSequence";
	else
		{
		build();
		ob->out(os);
		}
	}

Value SuSeq::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value ITER("Iter");
	static Value COPY("Copy");
	static Value JOIN("Join");
	static Value CLOSE("Close");
	static Value Instantiated("Instantiated?");

	if (member == Instantiated)
		{
		return ob ? SuTrue : SuFalse;
		}
	if (member == CLOSE)
		{
		return Value();
		}
	if (! ob && (! duped || infinite(iter))) // instantiate rather than dup again
		{
		if (member == ITER)
			{
			return dup();
			}
		else if (member == JOIN)
			{
			return Join(nargs);
			}
		else if (member == COPY)
			{
			// avoid two copies (build & copy)
			// for common usage: for m in ob.Members().Copy()
			return copy(dup());
			}

		static ushort G_Objects = globals("Sequences");
		if (Value Objects = globals.find(G_Objects))
			if (SuObject* obs = Objects.ob_if_ob())
				if (obs->hasMethod(member))
					return obs->call(self, member, nargs, nargnames, argnames, each);
		}
	build();
	return ob->call(self, member, nargs, nargnames, argnames, each);
	}

Value SuSeq::Join(short nargs) const
	{
	gcstring separator;
	if (nargs == 1)
		separator = ARG(0).gcstr();
	else if (nargs != 0)
		except("usage: object.Join(separator = '')");
	OstreamStr oss;
	Value x;
	bool first = true;
	Value newiter = dup();
	while (newiter != (x = next(newiter)))
		{
		if (first)
			first = false;
		else
			oss << separator;
		if (SuString* ss = val_cast<SuString*>(x))
			oss << ss->gcstr();
		else
			oss << x;
		}
	return new SuString(oss.gcstr());
	}

bool SuSeq::infinite(Value it)
	{
	static Value Infinite("Infinite?");

	KEEPSP
	try
		{
		return it.call(it, Infinite).toBool();
		}
	catch (...)
		{
		return false;
		}
	}

void SuSeq::build() const
	{
	if (!ob)
		ob = copy(iter);
	}

SuObject* SuSeq::copy(Value it)
	{
	if (infinite(it))
		except("can't instantiate infinite sequence");
	SuObject* copy = new SuObject;
	Value x;
	while (it != (x = next(it)))
		copy->add(x);
	return copy;
	}

Value SuSeq::dup() const
	{
	static Value DUP("Dup");

	duped = true;
	KEEPSP
	return iter.call(iter, DUP);
	}

Value SuSeq::next(Value it)
	{
	static Value NEXT("Next");

	KEEPSP
	Value x = it.call(it, NEXT);
	if (!x)
		except("no return value from sequence.Next");
	return x;
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

class Value SuSeq::rangeTo(int i, int j)
	{
	build();
	return ob->rangeTo(i, j);
	}

Value SuSeq::rangeLen(int i, int n)
	{
	build();
	return ob->rangeLen(i, n);
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

// SuSeqIter - implements Seq() ===============================================

class SuSeqIter : public SuValue
	{
	public:
		SuSeqIter(Value from, Value to, Value by);
		void out(Ostream& os) const override;
		Value call(Value self, Value member,
			short nargs, short nargnames, ushort* argnames, int each) override;
	private:
		Value from;
		Value to;
		Value by;
		Value i;
	};

SuSeqIter::SuSeqIter(Value f, Value t, Value b) : from(f), to(t), by(b)
	{
	i = from;
	}

void SuSeqIter::out(Ostream& os) const
	{
	os << "Seq";
	}

Value SuSeqIter::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NEXT("Next");
	static Value DUP("Dup");
	static Value Infinite("Infinite?");

	if (member == NEXT)
		{
		if (i >= to)
			return this;
		Value val = i;
		i = i + by;
		return val;
		}
	else if (member == DUP)
		return new SuSeqIter(from, to, by);
	else if (member == Infinite)
		return to == INT_MAX ? SuTrue : SuFalse;
	else
		method_not_found(type(), member);
	}

#include "prim.h"

Value su_seq()
	{
	const int nargs = 3;
	Value from = ARG(0);
	Value to = ARG(1);
	Value by = ARG(2);
	if (from == SuFalse)
		{
		from = 0;
		to = INT_MAX;
		}
	else if (to == SuFalse)
		{
		to = from;
		from = 0;
		}
	return new SuSeq(new SuSeqIter(from, to, by));
	}
PRIM(su_seq, "Seq(from = false, to = false, by = 1)");

// Sequence =========================================================

Value su_sequence()
	{
	const int nargs = 1;
	return new SuSeq(ARG(0));
	}
PRIM(su_sequence, "Sequence(iter)");

Value su_seq_q()
	{
	const int nargs = 1;
	return val_cast<SuSeq*>(ARG(0)) ? SuTrue : SuFalse;
	}
PRIM(su_seq_q, "Seq?(value)");
