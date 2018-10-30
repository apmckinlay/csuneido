#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// ReSharper disable CppUnusedIncludeDirective
#include "std.h"
#include <cstddef> // for size_t

class Value;
class gcstring;
class Ostream;
class SuNumber;
class SuObject;
struct Named;

// abstract base class for user accessible values
class SuValue {
public:
	virtual ~SuValue() = default;

	virtual void out(Ostream&) const = 0;
	virtual void show(Ostream& os) const {
		out(os);
	}

	virtual size_t hashfn() const;
	virtual size_t hashcontrib() const;

	virtual short symnum() const; // creates if necessary

	virtual const char* str_if_str() const; // defined by SuString
	virtual gcstring gcstr() const;         // no coerce
	virtual gcstring to_gcstr() const;      // defined by SuBoolean and SuNumber

	virtual bool int_if_num(int* pn) const; // only defined by SuNumber
	virtual int integer() const;            // only coerces "" and false to 0
	virtual SuNumber* number();             // only coerces "" and false to 0

	virtual SuObject* ob_if_ob(); // defined by SuObject & SuSeq
	SuObject* object();

	virtual Value call(Value self, Value member, short nargs = 0,
		short nargnames = 0, short* argnames = nullptr, int each = -1);

	virtual Value getdata(Value);
	virtual void putdata(Value, Value);

	virtual Value rangeTo(int i, int j);
	virtual Value rangeLen(int i, int n);

	virtual size_t packsize() const;
	virtual void pack(char* buf) const;
	gcstring pack() const;

	// WARNING: order-able types must define both order and lt
	virtual int order() const;
	virtual bool lt(const SuValue& y) const;
	virtual bool eq(const SuValue& y) const;

	virtual const char* type() const;
	virtual const Named* get_named() const;
};

int order(const char* name);

Ostream& operator<<(Ostream& out, SuValue* x);

gcstring packint(int x);
