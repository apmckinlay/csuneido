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

#include "sublock.h"
#include "suvalue.h"
#include "interp.h"
#include "sufunction.h"
#include "ostream.h"
#include "func.h"

class SuBlock : public Func
	{
public:
	SuBlock(Frame* f, int p, int fi, int n) : frame(f), fn(f->fn), pc(p), first(fi),
		persisted(false)
		{
		verify(frame->fn); // i.e. not a primitive
		if (n != BLOCK_REST)
			nparams = n;
		else
			{
			nparams = 1;
			rest = true;
			}
		literals = frame->fn->literals;
		locals = frame->fn->locals + first;
		// strip '_' prefix off params (first time)
		// if compile used a different way to hide block params this wouldn't be needed
		for (int i = 0; i < nparams; ++i)
			{
			auto s = symstr(locals[i]);
			if (*s != '_')
				break ; // already stripped previously
			locals[i] = ::symnum(s + 1);
			}
		}
	void out(Ostream& out) override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	void persist();
	friend Value suBlock(Frame* frame, int pc, int first, int nparams);
private:
	Frame* frame;
	SuFunction* fn; // copy of what's in frame, to detect if we missed persist
	int pc;
	short first;
	bool persisted;
	};

inline bool sameframe(Frame* f1, Frame* f2)
	{
	return f1->fn == f2->fn && f1->self == f2->self && f1->local == f2->local &&
		f1->rule.rec == f2->rule.rec && f1->rule.mem == f2->rule.mem;
	}

Value suBlock(Frame* frame, int pc, int first, int nparams)
	{
	return new SuBlock(frame, pc, first, nparams);
	}

void SuBlock::out(Ostream& out)
	{
	out << "/* block */";
	}

Value SuBlock::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL)
		{
		if (frame->fn != fn)
			except_err("orphaned block!");
		args(nargs, nargnames, argnames, each);
		Framer framer(frame, pc, first, nparams, self.ptr() == this ? frame->self : self);
		return tls().proc->fp->run();
		}
	else
		return Func::call(self, member, nargs, nargnames, argnames, each);
	}

void persist_if_block(Value x)
	{
	if (SuBlock* block = val_cast<SuBlock*>(x))
		block->persist();
	}

#define within(ob, p) \
	((char*) &(ob) < (char*) (p) && (char*) (p) < (char*) &(ob) + sizeof ob)

void SuBlock::persist()
	{
	if (persisted)
		return;

	if (frame->fn != fn)
		except_err("orphaned block!");

	// won't need to copy locals if another block persist has already done so
	if (within(tls().proc->stack, frame->local))
		{
		// save locals on heap - only done once per call/frame
		Value* old_locals = frame->local;
		int n = frame->fn->nlocals * sizeof (Value);
		frame->local = (Value*) memcpy(new char[n], frame->local, n);

		// update any other active frames pointing to the same locals
		// i.e. nested blocks within one parent frame
		for (Frame* f = tls().proc->fp; f > tls().proc->frames; --f)
			if (f->local == old_locals)
				f->local = frame->local;
		}

	// save frame on heap
	frame = (Frame*) memcpy(new Frame, frame, sizeof (Frame));

	persisted = true;
	}

#include "testing.h"

const char* eval(const char* s); // in builtins.cpp

class test_block : public Tests
	{
	TEST(0, simple)
		{
		auto s = eval("\
			s = '';\
			b = { s $= 'abc' };\
			b();\
			s");
		if (0 != strcmp(s, "abc"))
			except("'" << s << "'" << " != " << "'abc'");
		}
	TEST(1, persist1)
		{
		auto s = eval("\
			s = '';\
			b = { Object(b); s $= 'abc' };\
			b();\
			s");
		if (0 != strcmp(s, "abc"))
			except("'" << s << "'" << " != " << "'abc'");
		}
	TEST(2, persist2)
		{
		auto s = eval("\
			s = '';\
			persist = function (b) { Object(b) };\
			b = { persist(b); s $= 'abc' };\
			b();\
			s");
		if (0 != strcmp(s, "abc"))
			except("'" << s << "'" << " != " << "'abc'");
		}
	TEST(3, persist3)
		{
		auto s = eval("\
			doit = function (block) { block() };\
			wrap = function (@args) { 'abc' };\
			s = '';\
			doit() { s = wrap() { } };\
			s");
		if (0 != strcmp(s, "abc"))
			except("'" << s << "'" << " != " << "'abc'");
		}
	TEST(4, blockreturn)
		{
		auto s = eval("\
			b = { return 'good' };\
			b();\
			return 'bad'");
		if (0 != strcmp(s, "good"))
			except("'" << s << "'" << " != " << "'good'");
		}
	};
REGISTER(test_block);
