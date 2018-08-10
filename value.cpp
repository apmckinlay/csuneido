// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "gcstring.h"
#include "ostream.h"
#include "suboolean.h"
#include "except.h"

bool Value::toBool() const {
	if (p == SuBoolean::t)
		return true;
	else if (p == SuBoolean::f)
		return false;
	else
		except("expected boolean, got " << type());
}

int Value::index() const {
	int n;
	if (is_int())
		n = im.n;
	else if (!int_if_num(&n))
		except("indexes must be integers");
	return n;
}

Ostream& operator<<(Ostream& os, Value x) {
	if (x.is_int())
		os << x.im.n;
	else if (x.p)
		x.p->out(os);
	else
		os << "NULL";
	return os;
}

bool operator==(Value x, Value y) {
	if (x.p == y.p)
		return true;
	if (!x.p || !y.p)
		return false;
	return x.is_int()                        //  X    Y
		? y.is_int()                         //
			? false                          // int	int
			: NUM(x.im.n)->eq(*y.p)          // int  val
		: y.is_int() ? x.p->eq(*NUM(y.im.n)) // val	int
					 : x.p->eq(*y.p);        // val	val
}

bool operator<(Value x, Value y) {
	return x.is_int() ? y.is_int() ? x.im.n < y.im.n : NUM(x.im.n)->lt(*y.p)
					  : y.is_int() ? x.p->lt(*NUM(y.im.n)) : x.p->lt(*y.p);
}

Value Value::operator-() const {
	return is_int() ? Value(-im.n) : Value(neg(p->number()));
}

Value operator+(Value x, Value y) {
	return x.is_int() ? y.is_int() ? Value(x.im.n + y.im.n)
								   : Value(add(NUM(x.im.n), y.p->number()))
					  : y.is_int() ? Value(add(x.p->number(), NUM(y.im.n)))
								   : Value(add(x.p->number(), y.p->number()));
}

Value operator-(Value x, Value y) {
	return x.is_int() ? y.is_int() ? Value(x.im.n - y.im.n)
								   : Value(sub(NUM(x.im.n), y.p->number()))
					  : y.is_int() ? Value(sub(x.p->number(), NUM(y.im.n)))
								   : Value(sub(x.p->number(), y.p->number()));
}

Value operator*(Value x, Value y) {
	return x.is_int() ? y.is_int() ? Value(x.im.n * y.im.n)
								   : Value(mul(NUM(x.im.n), y.p->number()))
					  : y.is_int() ? Value(mul(x.p->number(), NUM(y.im.n)))
								   : Value(mul(x.p->number(), y.p->number()));
}

Value operator/(Value x, Value y) {
	return x.is_int() ? y.is_int() ? y.im.n != 0 && (x.im.n % y.im.n) == 0
				? Value(x.im.n / y.im.n)
				: Value(div(NUM(x.im.n), NUM(y.im.n)))
								   : Value(div(NUM(x.im.n), y.p->number()))
					  : y.is_int() ? Value(div(x.p->number(), NUM(y.im.n)))
								   : Value(div(x.p->number(), y.p->number()));
}

#include "catstr.h"

void cantforce(const char* t1, const char* t2) {
	if (has_prefix(t2, "class "))
		t2 += 6;
	if (has_prefix(t2, "Su"))
		t2 += 2;
	if (has_suffix(t2, "*"))
		t2 = PREFIXA(t2, strlen(t2) - 1);
	except("can't convert " << t1 << " to " << t2);
}

#include "sustring.h"

void method_not_found(const char* type, Value member) {
	if (val_cast<SuString*>(member))
		except("method not found: " << type << '.' << member.gcstr());
	else
		except("method not found: " << type << '.' << member);
}

// tests ------------------------------------------------------------

#include "testing.h"

TEST(value) {
	Value zero(0);
	verify(zero == zero);
	Value one(1);
	verify(one == one);
	Value mid(30000);
	verify(mid == mid);
	Value big(100000);
	verify(big == big);
	Value sym("sym");
	verify(sym == sym);
	Value str(new SuString("sym"));
	verify(str == str);

	verify(zero != one);
	verify(one != sym);
	verify(sym == str);
	verify(sym.hash() == str.hash());

	verify(zero < one);
	verify(one < big);
	verify(one < sym);

	Value x;

	assert_eq(x = zero + one, one);
	verify(x.is_int());
	assert_eq(x = one + one, 2);
	verify(x.is_int());
	assert_eq(x = mid + one, 30001);
	verify(x.is_int());
	verify(mid + mid == 60000); // overflow
	assert_eq(big + one, 100001);
	assert_eq(big + big, 200000);
	assert_eq(zero - mid - mid, -60000); // overflow

	assert_eq(Value(1) / Value(8), Value(new SuNumber(".125")));
	assert_eq(x = mid / Value(300), 100);
	verify(x.is_int());

	assert_eq(x = mid * mid, Value(30000 * 30000)); // overflow
}

TEST(value_smallint) {
	Value x(0x1234);
	assert_eq((int) x.p, 0x1234ffff);
}

#include "porttest.h"
#include "compile.h"
#include "ostreamstr.h"

PORTTEST(compare) {
	int n = args.size();
	for (int i = 0; i < n; ++i) {
		Value x = compile(args[i].str());
		// ReSharper disable once CppIdenticalOperandsInBinaryExpression
		if (x < x)
			return OSTR("\n\t" << x << " less than itself");
		for (int j = i + 1; j < n; ++j) {
			Value y = compile(args[j].str());
			if (!(x < y) || (y < x))
				return OSTR("\n\t" << x << " <=> " << y);
		}
	}
	return nullptr;
}
