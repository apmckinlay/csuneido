// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sublock.h"
#include "suvalue.h"
#include "interp.h"
#include "sufunction.h"
#include "ostream.h"
#include "func.h"

class SuBlock : public Func {
public:
	SuBlock(Frame* f, int p, int fi, int n) : frame(f), pc(p), first(fi) {
		verify(frame->fn); // i.e. not a primitive
		frame->created_block = true;
		if (n != BLOCK_REST)
			nparams = n;
		else {
			nparams = 1;
			rest = true;
		}
		literals = frame->fn->literals;
		locals = frame->fn->locals + first;
		persist();
	}
	void out(Ostream& out) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void persist();
	friend Value suBlock(Frame* frame, int pc, int first, int nparams);

private:
	Frame* frame;
	int pc;
	short first;
};

Value suBlock(Frame* frame, int pc, int first, int nparams) {
	return new SuBlock(frame, pc, first, nparams);
}

void SuBlock::out(Ostream& out) const {
	out << "/* block */";
}

Value SuBlock::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member == CALL) {
		args(nargs, nargnames, argnames, each);
		Framer framer(
			frame, pc, first, nparams, self.ptr() == this ? frame->self : self);
		return tls().proc->fp->run();
	} else
		return Func::call(self, member, nargs, nargnames, argnames, each);
}

#define within(ob, p) \
	((char*) &(ob) < (char*) (p) && (char*) (p) < (char*) &(ob) + sizeof(ob))

void SuBlock::persist() {
	// won't need to copy locals if another block persist has already done so
	if (within(tls().proc->stack, frame->local)) {
		// save locals on heap - only done once per call/frame
		Value* old_locals = frame->local;
		int n = frame->fn->nlocals * sizeof(Value);
		frame->local = (Value*) memcpy(new char[n], frame->local, n);

		// update any other active frames pointing to the same locals
		// i.e. nested blocks within one parent frame
		for (Frame* f = tls().proc->fp; f > tls().proc->frames; --f)
			if (f->local == old_locals)
				f->local = frame->local;
	}

	// save frame on heap
	frame = (Frame*) memcpy(new Frame, frame, sizeof(Frame));
}

// tests ------------------------------------------------------------

#include "testing.h"

const char* eval(const char* s); // in builtins.cpp

TEST(sublock_simple) {
	auto s = eval("\
		s = '';\
		b = { s $= 'abc' };\
		b();\
		s");
	if (0 != strcmp(s, "abc"))
		except("'" << s << "'"
				   << " != "
				   << "'abc'");
}

TEST(sublock_persist1) {
	auto s = eval("\
		s = '';\
		b = { Object(b); s $= 'abc' };\
		b();\
		s");
	if (0 != strcmp(s, "abc"))
		except("'" << s << "'"
				   << " != "
				   << "'abc'");
}

TEST(sublock_persist2) {
	auto s = eval("\
		s = '';\
		persist = function (b) { Object(b) };\
		b = { persist(b); s $= 'abc' };\
		b();\
		s");
	if (0 != strcmp(s, "abc"))
		except("'" << s << "'"
				   << " != "
				   << "'abc'");
}

TEST(sublock_persist3) {
	auto s = eval("\
		doit = function (block) { block() };\
		wrap = function (@args) { 'abc' };\
		s = '';\
		doit() { s = wrap() { } };\
		s");
	if (0 != strcmp(s, "abc"))
		except("'" << s << "'"
				   << " != "
				   << "'abc'");
}

TEST(sublock_blockreturn) {
	auto s = eval("\
		b = { return 'good' };\
		b();\
		return 'bad'");
	if (0 != strcmp(s, "good"))
		except("'" << s << "'"
				   << " != "
				   << "'good'");
}
