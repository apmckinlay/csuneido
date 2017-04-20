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

#include "value.h"
#include "gcstring.h"
#include "ostream.h"

const char* Value::str() const
	{
	return VAL->gcstr().str();
	}

gcstring Value::gcstr() const
	{
	return VAL->gcstr();
	}

gcstring Value::pack() const
	{
	return VAL->pack();
	}

Ostream& operator<<(Ostream& os, Value x)
	{
	if (x.is_int())
		os << x.im.n;
	else if (x.p)
		x.p->out(os);
	else
		os << "NULL";
	return os;
	}

bool operator==(Value x, Value y)
	{
	if (x.p == y.p)
		return true;
	if (! x.p || ! y.p)
		return false;
	return
		x.is_int()							//  X    Y
			? y.is_int()					// int	int
				? false
				: NUM(x.im.n)->eq(*y.p)		// int  val
			: y.is_int()
				? x.p->eq(*NUM(y.im.n))		// val	int
				: x.p->eq(*y.p);			// val	val
	}

bool operator<(Value x, Value y)
	{
	return
		x.is_int()
			? y.is_int()
				? x.im.n < y.im.n
				: NUM(x.im.n)->lt(*y.p)
			: y.is_int()
				? x.p->lt(*NUM(y.im.n))
				: x.p->lt(*y.p);
	}

Value Value::operator-() const
	{
	return is_int() ? (Value) Value(-im.n) : (Value) neg(p->number());
	}

Value operator+(Value x, Value y)
	{
	return
		x.is_int()
			? y.is_int()
				? Value(x.im.n + y.im.n)
				: Value(add(NUM(x.im.n), y.p->number()))
			: y.is_int()
				? Value(add(x.p->number(), NUM(y.im.n)))
				: Value(add(x.p->number(), y.p->number()));
	}

Value operator-(Value x, Value y)
	{
	return
		x.is_int()
			? y.is_int()
				? Value(x.im.n - y.im.n)
				: Value(sub(NUM(x.im.n), y.p->number()))
			: y.is_int()
				? Value(sub(x.p->number(), NUM(y.im.n)))
				: Value(sub(x.p->number(), y.p->number()));
	}

Value operator*(Value x, Value y)
	{
	return
		x.is_int()
			? y.is_int()
				? Value(x.im.n * y.im.n)
				: Value(mul(NUM(x.im.n), y.p->number()))
			: y.is_int()
				? Value(mul(x.p->number(), NUM(y.im.n)))
				: Value(mul(x.p->number(), y.p->number()));
	}

Value operator/(Value x, Value y)
	{
	return
		x.is_int()
			? y.is_int()
				? y.im.n != 0 && (x.im.n % y.im.n) == 0
					? Value(x.im.n / y.im.n)
					: Value(div(NUM(x.im.n), NUM(y.im.n)))
				: Value(div(NUM(x.im.n), y.p->number()))
			: y.is_int()
				? Value(div(x.p->number(), NUM(y.im.n)))
				: Value(div(x.p->number(), y.p->number()));
	}


#include "catstr.h"
#include "except.h"

void cantforce(const char* t1, const char* t2)
	{
	if (has_prefix(t2, "class "))
		t2 += 6;
	if (has_prefix(t2, "Su"))
		t2 += 2;
	if (has_suffix(t2, "*"))
		t2 = PREFIXA(t2, strlen(t2) - 1);
	except("can't convert " << t1 << " to " << t2);
	}

#include "sustring.h"

void method_not_found(const char* type, Value member)
	{
	if (val_cast<SuString*>(member))
		except("method not found: " << type << '.' << member.gcstr());
	else
		except("method not found: " << type << '.' << member);
	}

// test =============================================================

#include "testing.h"

class test_value : public Tests
	{
	TEST(0, main)
		{
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

		asserteq(x = zero + one, one);
		verify(x.is_int());
		asserteq(x = one + one, 2);
		verify(x.is_int());
		asserteq(x = mid + one, 30001);
		verify(x.is_int());
		verify(mid + mid == 60000); // overflow
		asserteq(big + one, 100001);
		asserteq(big + big, 200000);
		asserteq(zero - mid - mid, -60000); // overflow

		asserteq(Value(1) / Value(8), Value(new SuNumber(".125")));
		asserteq(x = mid / Value(300), 100);
		verify(x.is_int());

		asserteq(x = mid * mid, Value(30000 * 30000)); // overflow
		}
	};
REGISTER(test_value);
