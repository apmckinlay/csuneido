#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"
#include "sunumber.h"
#include "symbols.h"
#include "gcstring.h"
#include <malloc.h>
#include <typeinfo>

class SuString;
class SuObject;
class Ostream;

// allocates a temporary SuNumber on the stack
// NOTE: only for use by Value, not intended to be public
#define NUM(n) (new (_alloca(sizeof(SuNumber))) SuNumber(n))

// return a SuValue pointer, temporary auto box integer on stack if necessary
// NOTE: only for use by Value, not intended to be public
#define VAL ((SuValue*) (is_int() ? NUM(im.n) : p))

/*
 * Value is a "value" type that either directly contains a short integer
 * or else wraps an SuValue* which handles polymorphism.
 * Assumes there are no valid pointer with a low short of 0xffff
 * which should be safe since alignment should be at least even.
 */
class Value {
public:
	Value() : p(nullptr) { // NOLINT
	}
	Value(SuValue* x) : p(x) { // NOLINT
	}
	Value(const SuString* x) : p((SuValue*) x) { // NOLINT
	}
	Value(int n) { // NOLINT
		if (SHRT_MIN <= n && n <= SHRT_MAX) {
			im.type = INTVAL;
			im.n = n;
		} else
			p = new SuNumber(n);
	}
	Value(const char* s) : p(symbol(s).p) { // NOLINT
	}

	SuValue* ptr() const {
		return is_int() ? nullptr : p;
	}
	// to allow if(val), checks for non null, not true or false
	explicit operator bool() const {
		return p;
	}

	// throws
	bool toBool() const;

	unsigned int hash() const {
		return is_int() ? im.n : p->hashfn();
	}
	unsigned int hashcontrib() const {
		return is_int() ? im.n : p->hashcontrib();
	}

	// does NOT accept false or "" as zero
	[[nodiscard]] bool int_if_num(int* pn) const {
		return is_int() ? (*pn = im.n, true) : p->int_if_num(pn);
	}

	// throws, accepts false and "" as zero
	int integer() const {
		return is_int() ? im.n : (p ? p->integer() : 0);
	}

	// throws, does NOT accept false or "" as zero
	int index() const;

	// accepts false and "" as zero
	SuNumber* number() const {
		return is_int() ? new SuNumber(im.n) : p->number();
	}

	int symnum() const {
		return is_int() && im.n > 0 ? im.n : VAL->symnum();
	}

	const char* str_if_str() const {
		return VAL->str_if_str();
	}
	gcstring gcstr() const { // only if string
		return VAL->gcstr();
	}
	gcstring to_gcstr() const { // coerces boolean, number, object-with-ToString
		return VAL->to_gcstr();
	}
	const char* str() const {
		return VAL->gcstr().str();
	}

	SuObject* object() const {
		return VAL->object();
	}
	SuObject* ob_if_ob() const {
		return is_int() || !p ? nullptr : p->ob_if_ob();
	}

	Value call(Value self, Value member, short nargs = 0, short nargnames = 0,
		short* argnames = nullptr, int each = -1) {
		return VAL->call(self, member, nargs, nargnames, argnames, each);
	}

	Value getdata(Value m) const {
		return VAL->getdata(m);
	}
	void putdata(Value m, Value x) {
		VAL->putdata(m, x);
	}

	Value rangeTo(int i, int j) {
		return VAL->rangeTo(i, j);
	}
	Value rangeLen(int i, int n) {
		return VAL->rangeLen(i, n);
	}

	size_t packsize() const {
		return VAL->packsize();
	}
	//		{ return is_int() ? ::packsize(im.n) : VAL->packsize(); }
	void pack(char* buf) const {
		VAL->pack(buf);
	}
	//		{ is_int() ? packint(buf, im.n) : VAL->pack(buf); }
	gcstring pack() const {
		return VAL->pack();
	}
	//		{ return is_int() ? ::packint(im.n) : VAL->pack(); }

	const char* type() const {
		return is_int() ? "Number" : p ? VAL->type() : "null";
	}
	const Named* get_named() const {
		return is_int() || !p ? nullptr : p->get_named();
	}
	bool sameAs(Value other) const {
		return p == other.p;
	}

	Value operator-() const; // unary minus
	Value operator+() const; // unary plus
	friend bool operator==(Value x, Value y);
	friend bool operator<(Value x, Value y);
	friend Value operator+(Value x, Value y);
	friend Value operator-(Value x, Value y);
	friend Value operator*(Value x, Value y);
	friend Value operator/(Value x, Value y);
	friend Ostream& operator<<(Ostream& os, Value x);

	bool is_int() const {
		return im.type == INTVAL;
	}

private:
	static const short INTVAL = static_cast<uint16_t>(0xffff);
	union {
		SuValue* p;
		struct {
			short type; // NOTE: should be low word
			short n;
		} im;
	};
	friend void test_value_smallint();
};

template <class T>
inline T val_cast(Value v) {
	return dynamic_cast<T>(v.ptr());
}

[[noreturn]] void cantforce(const char* t1, const char* t2);

template <class T>
T force(Value x) {
	if (T t = val_cast<T>(x))
		return t;
	cantforce(x ? x.type() : "NULL", typeid(T).name());
}

template <class T>
struct HashFn;

template <>
struct HashFn<Value> {
	unsigned int operator()(Value x) const {
		return x.hash();
	}
};

extern Value NEW;
extern Value CALL;
extern Value PARAMS;
extern Value CALL_CLASS;
extern Value CALL_INSTANCE;
extern Value INSTANTIATE;
extern Value SuOne;
extern Value SuZero;
extern Value SuMinusOne;
extern Value SuTrue;
extern Value SuFalse;
extern Value SuEmptyString;

[[noreturn]] void method_not_found(const char* type, Value member);

// implementation in suclass.cpp
class UserDefinedMethods {
public:
	UserDefinedMethods(const char* name);

	Value operator()(Value member) const;

private:
	uint16_t gnum;
};
