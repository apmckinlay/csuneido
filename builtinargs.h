#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "gcstring.h"
#include "list.h"
#include "suobject.h" // not ideal, but needed for SuObject::iterator
#include <optional>

class ArgsIter {
public:
	ArgsIter(short n, short na, short* an, int each);

	Value getval(const char* name);
	Value getNamed(const char* name);
	Value getNextUnnamed();
	bool hasMoreUnnamed() const;
	Value getNext();
	Value call(Value fn, Value self, Value method);

private:
	friend class BuiltinArgs;
	Value arg(int i) const;
	Value next();

	short nargs;
	short nargnames;
	short* argnames;
	int each;
	int unnamed;
	SuObject* ob = nullptr;
	std::optional<SuObject::iterator> oi;
	Value curname;
	int i = 0;
	ListSet<Value> taken;
};

class BuiltinArgs {
public:
	BuiltinArgs(short nargs, short nargnames, short* argnames, int each)
		: ai(nargs, nargnames, argnames, each) {
	}

	BuiltinArgs& usage(
		const char* s1, const char* s2 = "", const char* s3 = "") {
		msg1 = s1;
		msg2 = s2;
		msg3 = s3;
		return *this;
	}
	void exceptUsage() const;
	Value getValue(const char* name);
	Value getValue(const char* name, Value defval) {
		def = true;
		Value val = getval(name);
		return val ? val : defval;
	}
	int getint(const char* name) {
		ckndef();
		return getValue(name).integer();
	}
	int getint(const char* name, int defval) {
		def = true;
		Value val = getval(name);
		return val ? val.integer() : defval;
	}
	const char* getstr(const char* name) {
		ckndef();
		return getValue(name).str();
	}
	const char* getstr(const char* name, const char* defval) {
		def = true;
		Value val = getval(name);
		return val ? val.str() : defval;
	}
	gcstring getgcstr(const char* name) {
		ckndef();
		return getValue(name).gcstr();
	}
	gcstring getgcstr(const char* name, gcstring defval) {
		def = true;
		Value val = getval(name);
		return val ? val.gcstr() : defval;
	}
	Value getNamed(const char* name) {
		return ai.getNamed(name);
	}
	bool hasNamed() const {
		return ai.nargnames > 0;
	}
	Value getNext() {
		return ai.getNext();
	}
	Value curName() const {
		return ai.curname;
	}
	Value getNextUnnamed() {
		return ai.getNextUnnamed();
	}
	void end() const;
	bool hasUnnamed() const {
		return ai.unnamed > 0;
	}
	Value call(Value fn, Value self, Value method) {
		return ai.call(fn, self, method);
	}
	int n_args() const {
		return ai.nargs;
	}
	int n_argnames() const {
		return ai.nargnames;
	}
	int n_unnamed() const {
		return ai.unnamed;
	}

private:
	Value getval(const char* name) {
		return ai.getval(name);
	}
	void ckndef() const;

	ArgsIter ai;
	const char* msg1 = "invalid arguments";
	const char* msg2 = "";
	const char* msg3 = "";
	bool def = false;
};
