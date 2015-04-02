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

#include "except.h"
#include "exceptimp.h"
#include "interp.h"
#include "ostreamstr.h"
#include "trace.h"
#include "suobject.h"
#include "sufunction.h"

SuObject* copyCallStack();

Except::Except(gcstring x) : SuString(x.trim()), 
	fp_fn_(tls().proc ? tls().proc->fp->fn : 0), block_return(x == "block return")
	{
	if (x.has_prefix("block") &&
		(x == "block:continue" || x == "block:break" || block_return))
		{
		static SuObject* empty = new SuObject;
		calls_ =  empty;
		if (block_return)
			verify(fp_fn_);
		}
	else
		{
		TRACE(EXCEPTIONS, "throwing: " << x);
		calls_ = copyCallStack();
		}
	}

Except::Except(const Except& e, gcstring s)
	: SuString(s), fp_fn_(e.fp_fn_), calls_(e.calls_), block_return(e.block_return)
	{  }

Ostream& operator<<(Ostream& os, const Except& e)
	{
	return os << e.str();
	}

static OstreamStr os(200);

Ostream& osexcept()
	{
	return os;
	}

void except_()
	{
	Except e(os.str());
	os.clear();
	throw e;
	}

// to allow setting breakpoints
void except_err_()
	{
	Except e(os.str());
	os.clear();
	throw e;
	}

#define TRY(stuff) do try { stuff; } catch (...) { } while (false)

SuObject* copyCallStack()
	{
	static Value qqq = new SuString("???");
	SuObject* calls = new SuObject;
	if (! tls().proc || ! tls().proc->fp)
		return calls;
	for (Frame* f = tls().proc->fp; f > tls().proc->frames; --f)
		{
		SuObject* call = new SuObject;
		SuObject* vars = new SuObject;
		if (f->fn)
			{
			call->put("fn", f->fn);
			int i = 0, n = 0;
			TRY(i = f->fn->source(f->ip - f->fn->code - 1, &n));
			call->put("src_i", i);
			call->put("src_n", n);
			for (i = 0; i < f->fn->nlocals; ++i)
				if (f->local[i])
					{
					Value value = qqq;
					TRY(value = f->local[i]);
					TRY(vars->put(symbol(f->fn->locals[i]), value));
					}
			if (f->self)
				vars->put("this", f->self);
			}

		else if (f->prim)
			{
			TRY(call->put("fn", f->prim));
			}

		call->put("locals", vars);
		calls->add(call);
		}
	return calls;
	}

Value Except::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value As("As");
	static Value Callstack("Callstack");
	if (member == As)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: exception.As(string)");
		return new Except(*this, ARG(0).gcstr());
		}
	if (member == Callstack)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 0)
			except("usage: exception.Callstack()");
		return calls();
		}
	else
		return SuString::call(self, member, nargs, nargnames, argnames, each);
	}
