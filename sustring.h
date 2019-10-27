#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "suvalue.h"
#include "gcstring.h"

class SuNumber;

// string values - wraps gcstring
// immutable
class SuString : public SuValue {
public:
	explicit SuString(const gcstring& t) : s(t) {
	}
	explicit SuString(const char* t) : s(t) {
	}
	SuString(const char* p, size_t n) : s(p, n) {
	}
	explicit SuString(size_t n) : s(n) {
	}

	static SuString* noalloc(const char* t) {
		return new SuString(gcstring::noalloc(t));
	}
	static SuString* noalloc(const char* t, size_t n) {
		return new SuString(gcstring::noalloc(t, n));
	}

	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

	Value getdata(Value) override;
	Value rangeTo(int i, int j) override;
	Value rangeLen(int i, int n) override;

	const char* str_if_str() const override {
		return str();
	}

	int integer() const override;
	SuNumber* number() override;

	int size() const {
		return s.size();
	}

	size_t packsize() const override;
	void pack(char* buf) const override;
	static Value unpack(const gcstring& s);

	const char* ptr() const {
		return s.ptr();
	}

	const char* str() const {
		return s.str();
	}

	gcstring gcstr() const override {
		return s;
	}
	gcstring to_gcstr() const override {
		return s;
	}

	short symnum() const override;

	size_t hashfn() const override {
		return ::hashfn(s.ptr(), s.size());
	}

	const SuString* substr(size_t i, size_t n) const {
		return (i == 0 && n >= s.size()) ? this : new SuString(s.substr(i, n));
	}

	bool operator==(const char* t) const {
		return s == t;
	}

	void out(Ostream& out) const override;

	int order() const override;
	bool lt(const SuValue& x) const override;
	bool eq(const SuValue& x) const override;

	bool is_identifier() const;

	SuString* tolower() const;

protected:
	gcstring s;

private:
	Value Alphaq(short nargs, short nargnames, short* argnames, int each);
	Value AlphaNumq(short nargs, short nargnames, short* argnames, int each);
	Value Asc(short nargs, short nargnames, short* argnames, int each);
	Value Call(short nargs, short nargnames, short* argnames, int each);
	Value Compile(short nargs, short nargnames, short* argnames, int each);
	Value Count(short nargs, short nargnames, short* argnames, int each);
	Value Detab(short nargs, short nargnames, short* argnames, int each);
	Value Entab(short nargs, short nargnames, short* argnames, int each);
	Value Eval(short nargs, short nargnames, short* argnames, int each);
	Value Eval2(short nargs, short nargnames, short* argnames, int each);
	Value Extract(short nargs, short nargnames, short* argnames, int each);
	Value Find(short nargs, short nargnames, short* argnames, int each);
	Value FindLast(short nargs, short nargnames, short* argnames, int each);
	Value Find1of(short nargs, short nargnames, short* argnames, int each);
	Value FindLast1of(short nargs, short nargnames, short* argnames, int each);
	Value Findnot1of(short nargs, short nargnames, short* argnames, int each);
	Value FindLastnot1of(
		short nargs, short nargnames, short* argnames, int each);
	Value Hasq(short nargs, short nargnames, short* argnames, int each);
	Value Instantiate(short nargs, short nargnames, short* argnames, int each);
	Value Iter(short nargs, short nargnames, short* argnames, int each);
	Value Lower(short nargs, short nargnames, short* argnames, int each);
	Value Lowerq(short nargs, short nargnames, short* argnames, int each);
	Value MapN(short nargs, short nargnames, short* argnames, int each);
	Value Match(short nargs, short nargnames, short* argnames, int each);
	Value Mbstowcs(short nargs, short nargnames, short* argnames, int each);
	Value NthLine(short nargs, short nargnames, short* argnames, int each);
	Value Numberq(short nargs, short nargnames, short* argnames, int each);
	Value Numericq(short nargs, short nargnames, short* argnames, int each);
	Value Prefixq(short nargs, short nargnames, short* argnames, int each);
	Value Repeat(short nargs, short nargnames, short* argnames, int each);
	Value Replace(short nargs, short nargnames, short* argnames, int each);
	Value replace(const gcstring& pat, Value rep, int count = INT_MAX) const;
	Value Reverse(short nargs, short nargnames, short* argnames, int each);
	Value ServerEval(short nargs, short nargnames, short* argnames, int each);
	Value Size(short nargs, short nargnames, short* argnames, int each);
	Value Split(short nargs, short nargnames, short* argnames, int each);
	Value Substr(short nargs, short nargnames, short* argnames, int each);
	Value Suffixq(short nargs, short nargnames, short* argnames, int each);
	Value Tr(short nargs, short nargnames, short* argnames, int each);
	Value Unescape(short nargs, short nargnames, short* argnames, int each);
	Value Upper(short nargs, short nargnames, short* argnames, int each);
	Value Upperq(short nargs, short nargnames, short* argnames, int each);
	Value Wcstombs(short nargs, short nargnames, short* argnames, int each);

	bool backquote() const;
	friend void test_sustring_replace();
};

class SuBuffer : public SuString {
public:
	SuBuffer(size_t n, const gcstring& s);

	char* buf() {
		return s.buf();
	}

	void adjust() { // set length to strlen
		s = s.substr(0, s.find('\0'));
	}

	bool used = false;
};

bool is_identifier(const char* s, int n = -1);
