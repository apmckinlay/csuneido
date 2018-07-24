// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"
#include "value.h"
#include "except.h"
#include "ostreamstr.h"
#include "gcstring.h"
#include "pack.h"
#include <typeinfo>
#include <cctype>

static int other = ::order("other");

int SuValue::order() const {
	return other;
}

bool SuValue::lt(const SuValue& y) const {
	// this has no lt so it must be other
	return order() == y.order() && this < &y;
} // default is simply compare addresses

bool SuValue::eq(const SuValue& y) const {
	return this == &y;
} // default is simply compare addresses

size_t SuValue::hashfn() const {
	return ((uint32_t) this >> 4);
} // default function just uses address

size_t SuValue::hashcontrib() const {
	return hashfn();
}

int SuValue::integer() const {
	except("can't convert " << type() << " to number");
}

gcstring SuValue::gcstr() const {
	except("can't convert " << type() << " to String");
}

gcstring SuValue::to_gcstr() const {
	except("can't convert " << type() << " to String");
}

short SuValue::symnum() const {
	except("not a valid member: " << type());
}

bool SuValue::int_if_num(int* pn) const {
	return false;
}

SuNumber* SuValue::number() {
	except("can't convert " << type() << " to number");
}

const char* SuValue::str_if_str() const {
	return 0;
}

SuObject* SuValue::ob_if_ob() {
	return 0;
}

SuObject* SuValue::object() {
	if (SuObject* ob = ob_if_ob())
		return ob;
	else
		except("can't convert " << type() << " to object");
}

Value SuValue::getdata(Value k) {
	except(type() << " does not support get (" << k << ")");
}

void SuValue::putdata(Value k, Value v) {
	except(type() << " does not support put (" << k << ")");
}

Value SuValue::rangeTo(int i, int j) {
	except(type() << " does not support range");
}

Value SuValue::rangeLen(int i, int n) {
	except(type() << " does not support range");
}

size_t SuValue::packsize() const {
	except("can't pack " << type());
}

void SuValue::pack(char* buf) const {
	except("can't pack " << type());
}

gcstring SuValue::pack() const {
	int n = packsize();
	char* buf = salloc(n);
	pack(buf);
	return gcstring::noalloc(buf, n);
}

gcstring packint(int x) {
	int n = packintsize(x);
	char* buf = salloc(n);
	packint(buf, x);
	return gcstring::noalloc(buf, n);
}

Ostream& operator<<(Ostream& out, SuValue* x) {
	if (x)
		x->out(out);
	else
		out << "NULL";
	return out;
}

Value SuValue::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member == CALL)
		except("can't call " << type());
	else
		method_not_found(type(), member);
}

int order(const char* name) {
	static const char* ord[] = {
		"Boolean", "Number", "String", "Date", "Object", "other"};
	const int nord = sizeof ord / sizeof(char*);
	for (int i = 0; i < nord; ++i)
		if (0 == strcmp(name, ord[i])) {
			ord[i] = "";
			return i;
		}
	error("unknown or duplicate type: " << name);
}

const char* SuValue::type() const {
	const char* s = typeid(*this).name();
	while (isdigit(*s))
		++s; // for gcc
	if (has_prefix(s, "class "))
		s += 6;
	if (has_prefix(s, "Su"))
		s += 2;
	if (0 == strcmp(s, "Symbol"))
		return "String";
	return s;
}

const Named* SuValue::get_named() const {
	return 0;
}
