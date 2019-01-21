// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sublock.h"
#include "suvalue.h"
#include "interp.h"
#include "sufunction.h"
#include "ostream.h"
#include "func.h"
#include "fatal.h"

class SuBlock : public Func {
public:
	SuBlock(Frame* f, int p, int fi, int n)
		: frame(f), fn(f->fn), pc(p), first(fi), persisted(false) {
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
	}
	void out(Ostream& out) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void persist();
	friend Value suBlock(Frame* frame, int pc, int first, int nparams);

private:
	Frame* frame;
	SuFunction* fn; // copy of what's in frame, to detect if we missed persist
	int pc;
	short first;
	bool persisted;
};

Value suBlock(Frame* frame, int pc, int first, int nparams) {
	return new SuBlock(frame, pc, first, nparams);
}

void SuBlock::out(Ostream& out) const {
	out << "/* block */";
}

class Persister {
public:
	explicit Persister(Frame* f) : frame(f) {
	}
	~Persister() {
		if (frame->created_block)
			for (int i = 0; i < frame->fn->nlocals; ++i)
				persist_if_block(frame->local[i]);
	}

private:
	Frame* frame;
};

Value SuBlock::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member == CALL) {
		if (frame->fn != fn)
			except_err("orphaned block!");
		args(nargs, nargnames, argnames, each);
		Framer framer(
			frame, pc, first, nparams, self.ptr() == this ? frame->self : self);
		// if a block creates another block
		// and stores it in a local (of its parent function)
		// then we need to persist it when we return
		// otherwise it escapes its creating frame and becomes orphaned
		Persister p(tls().proc->fp);
		return tls().proc->fp->run();
	} else
		return Func::call(self, member, nargs, nargnames, argnames, each);
}

void persist_if_block(Value x) {
	if (SuBlock* block = val_cast<SuBlock*>(x))
		block->persist();
}

#define within(ob, p) \
	((char*) &(ob) < (char*) (p) && (char*) (p) < (char*) &(ob) + sizeof(ob))

void SuBlock::persist() {
	if (persisted)
		return;

	if (frame->fn != fn)
		fatal("orphaned block!");

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

	persisted = true;
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
