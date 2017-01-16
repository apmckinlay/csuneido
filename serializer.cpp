/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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

#include "serializer.h"
#include "value.h"
#include "gcstring.h"
#include "sockets.h"
#include "except.h" // for verify
#include "pack.h"
#include "lisp.h"

#include "clientserver.h"
#include "ostreamcon.h"
#define LOG(stuff) con() << stuff << ' '
#define LOGSTR(s) LOG('"' << s.substr(0, 20) << (s.size() > 20 ? "..." : "") << '"')

Serializer::Serializer(Buffer& r, Buffer& w) : rdbuf(r), wrbuf(w)
	{ }

void Serializer::clear()
	{
	rdbuf.clear();
	wrbuf.clear();
	}

// writing -----------------------------------------------------------------------

Serializer& Serializer::put(char c)
	{
	LOG("'" << c << "'");
	wrbuf.add(c);
	return *this;
	}

Serializer& Serializer::putCmd(char c)
	{
	LOG(endl << "=> " << cmdnames[c]);
	wrbuf.add(c);
	return *this;
	}

Serializer& Serializer::putOk()
	{
	LOG(endl << "\tOK");
	wrbuf.add(1); // true
	return *this;
	}

void Serializer::putErr(const char* err)
	{
	LOG(endl << "\tERR " << err);
	wrbuf.add(0); // false
	}

Serializer& Serializer::putBool(bool b)
	{
	LOG((b ? "true" : "false"));
	wrbuf.add(b ? 1 : 0);
	return *this;
	}

Serializer& Serializer::putInt(int64_t i)
	{
	LOG(i);
	char* buf = wrbuf.ensure(10); // 64 bits / 7 bits per byte
	char* dst = buf;
	uint64_t n = (i << 1) ^ (i >> 63); // zig zag encoding
	while (n > 0x7f)
		{
		*dst++ = (n & 0x7f) | 0x80;
		n >>= 7;
		}
	*dst++ = n & 0x7f;
	verify(dst - buf < 10);
	wrbuf.added(dst - buf);
	return *this;
	}

Serializer& Serializer::putStr(const char* s)
	{
	return putStr(gcstring(strlen(s), s)); // no alloc
	}

Serializer& Serializer::putStr(const gcstring& s)
	{
	putInt(s.size());
	LOG('-');
	LOGSTR(s);
	wrbuf.add(s);
	return *this;
	}

Serializer& Serializer::putValue(Value value)
	{
	auto n = value.packsize();
	putInt(n);
	LOG("- " << value);
	value.pack(wrbuf.alloc(n));
	return *this;
	}

Serializer & Serializer::putInts(Lisp<int> list)
	{
	auto n = list.size();
	putInt(n);
	LOG('(');
	for (; !nil(list); ++list)
		putInt(*list);
	LOG(')');
	return *this;
	}

Serializer& Serializer::putStrings(Lisp<gcstring> list)
	{
	auto n = list.size();
	putInt(n);
	LOG('(');
	for (; ! nil(list); ++list)
		putStr(*list);
	LOG(')');
	return *this;
	}

// reading -----------------------------------------------------------------------

char Serializer::get1()
	{
	need(1);
	return rdbuf.get();
	}

char Serializer::get()
	{
	char c = get1();
	LOG("'" << c << "'");
	return c;
	}

char Serializer::getCmd()
	{
	char c = get1();
	LOG(endl << "<= " << cmdnames[c]);
	return c;
	}

bool Serializer::getBool()
	{
	switch (get1())
		{
	case 0: LOG("false"); return false;
	case 1: LOG("true"); return true;
	default:
		except("bad boolean value");
		}
	}

int64_t Serializer::getInt()
	{
	int shift = 0;
	int64_t n = 0;
	char b;
	do
		{
		b = get1();
		n |= int64_t(b & 0x7f) << shift;
		shift += 7;
		} while (b & 0x80);
	int64_t tmp = (((n << 63) >> 63) ^ n) >> 1;
	tmp = tmp ^ (n & (int64_t(1) << 63));
	LOG(tmp);
	return tmp;
	}

gcstring Serializer::getStr()
	{
	int n = getInt();
	LOG('-');
	need(n);
	gcstring s = rdbuf.getStr(n);
	LOGSTR(s);
	return s;
	}

gcstring Serializer::getBuf()
	{
	int n = getInt();
	LOG('-');
	gcstring s(n);
	read(s.buf(), n);
	LOGSTR(s);
	return s;
	}

Value Serializer::getValue()
	{
	// need to use a separate buffer because unpack assumes it owns the buffer
	// i.e. strings will just point into the buffer
	int n = getInt();
	LOG('-');
	gcstring v(n);
	read(v.buf(), n);
	Value val = unpack(v);
	LOG(val);
	return val;
	}

Lisp<int> Serializer::getInts()
	{
	int n = getInt();
	LOG('(');
	Lisp<int> list;
	for (int i = 0; i < n; ++i)
		list.push(getInt());
	LOG(')');
	return list.reverse();
	}

Lisp<gcstring> Serializer::getStrings()
	{
	int n = getInt();
	LOG('(');
	Lisp<gcstring> list;
	for (int i = 0; i < n; ++i)
		list.push(getStr());
	LOG(')');
	return list.reverse();
	}

// tests ------------------------------------------------------------

#include "testing.h"

#define test(n) buf.clear(); se.putInt(n); assert_eq(se.getInt(), n)

class TestSerializer : public Serializer
	{
public:
	TestSerializer(Buffer& buf) : Serializer(buf, buf)
		{
		}
protected:
	void need(int n) override
		{ verify(n <= rdbuf.remaining()); }
	void read(char* dst, int n) override
		{
		verify(n < rdbuf.remaining());
		memcpy(dst, rdbuf.getBuf(n), n);
		}
	};

class test_serializer : public Tests
	{
	TEST(0, main)
		{
		Buffer buf;
		TestSerializer se(buf);
		auto ints = lisp(123, 456);
		auto strings = lisp(gcstring("foo"), gcstring("bar"));

		test(0);
		test(55);
		test(-1);
		test(-55);
		test(123456789);
		test(-123456789);

		buf.clear();
		se.putBool(false);
		se.putBool(true);
		se.put('x');
		se.putInt(123);
		se.putStr("hello world");
		se.putStr(gcstring("foobar"));
		se.putValue(SuMinusOne);
		se.putInts(ints);
		se.putStrings(strings);

		assert_eq(se.getBool(), false);
		assert_eq(se.getBool(), true);
		assert_eq(se.get(), 'x');
		assert_eq(se.getInt(), 123);
		assert_eq(se.getStr(), "hello world");
		assert_eq(se.getStr(), "foobar");
		assert_eq(se.getValue(), SuMinusOne);
		assert_eq(se.getInts(), ints);
		assert_eq(se.getStrings(), strings);
		}
	};
REGISTER(test_serializer);
