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

#include "sustring.h"
#include "pack.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "suboolean.h"
#include "sunumber.h"
#include "suobject.h"
#include "regexp.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "symbols.h"
#include "minmax.h"
#include "gc.h"
/*#if defined(_MSC_VER) && _MSC_VER <= 1200
#include <locale>
using namespace std;
#endif*/

SuString* SuString::empty_string = new SuString("");

void SuString::out(Ostream& out)
	{
	extern bool obout_inkey;
	if (obout_inkey && is_identifier())
		out << s;
	else
		{
		char quote = (s.find('"') != -1 && s.find('\'') == -1) ? '\'' : '"';
		out << quote;
		char* buf = s.buf();
		size_t n = s.size();
		for (size_t i = 0; i < n; ++i)
			{
			uchar c = buf[i];
			if (c == 0)
				out << "\\x00";
			else if (c == quote || c == '\\')
				out << '\\' << c;
			else
				out << c;
			}
		out << quote;
		}
	}

bool is_identifier(const char* buf, int n)
	{
	if (n == -1)
		n = strlen(buf);
	if (n < 1 || (! isalpha(buf[0]) && buf[0] != '_'))
		return false;
	for (int i = 0; i < n; ++i)
		{
		char c = buf[i];
		if (! isalpha(c) && ! isdigit(c) && c != '_')
			return (c == '?' || c == '!') && i == n - 1 ? true : false;
		}
	return true;
	}

bool SuString::is_identifier() const
	{
	return ::is_identifier(s.buf(), s.size());
	}

int SuString::symnum() const
	{
	return ::symnum(str());
	}

SuNumber* SuString::number()
	{
	if (size() == 0)
		return &SuNumber::zero;
	char* s = str();
	return size() == numlen(s) ? new SuNumber(s, size()) : SuValue::number();
	}

static int ord = ::order("String");

int SuString::order() const
	{ return ord; }

bool SuString::eq(const SuValue& y) const
	{
	if (y.order() == ord)
		return s == static_cast<const SuString&>(y).s;
	else
		return false;
	}

bool SuString::lt(const SuValue& y) const
	{
	int yo = y.order();
	if (yo == ord)
		return s < static_cast<const SuString&>(y).s;
	else
		return ord < yo;
	}

SuBuffer::SuBuffer(size_t n, const gcstring& s) : SuString(n + 1)
	{ 
	memcpy(buf(), s.buf(), min(n, (size_t) s.size()));
	buf()[n] = 0;
	}

// packing ==========================================================

size_t SuString::packsize() const
	{
	int n = size();
	return n ? 1 + n : 0; 
	}

void SuString::pack(char* dst) const
	{ 
	int n = size();
	if (n == 0)
		return ;
	dst[0] = PACK_STRING;
	memcpy(dst + 1, buf(), n); 
	}

/*static*/ SuString* SuString::unpack(const gcstring& s)
	{
	if (s.size() <= 1)
		return SuString::empty_string;
	else
		return new SuString(s.substr(1));
	}

// literal ==========================================================

inline int hexval(char c)
	{ return c <= '9' ? c - '0' : tolower(c) - 'a' + 10; }

inline bool isodigit(char c)
	{ return '0' <= c && c <= '7'; }

static char doesc(const char*& s)
	{
	++s; // backslash
	switch (*s)
		{
	case 'n' :
		return '\n';
	case 't' :
		return '\t';
	case 'r' :
		return '\r';
	case 'x' :
		if (isxdigit(s[1]) && isxdigit(s[2]))
			{
			s += 2;
			return 16 * hexval(s[-1]) + hexval(s[0]);
			}
		else
			return *--s; // the backslash itself
	case '\\' :
	case '"' :
	case '\'' :
		return *s;
	default :
		if (isodigit(s[0]) && isodigit(s[1]) && isodigit(s[2]))
			{
			s += 2;
			return (s[0] - '0') + 8 * (s[-1] - '0') + 64 * (s[-2] - '0');
			}
		else
			return *--s; // the backslash itself
		}
	}

/* static */ SuString* SuString::literal(const char* s)
	{
	size_t n = strlen(s);
	if ((*s != '"' && *s != '\'') || *s != s[n - 1])
		return 0;
	char* buf = (char*) alloca(n);
	char* dst = buf;
	for (char quote = *s++; *s != quote; ++s)
		{
		if ('\\' == *s)
			*dst++ = doesc(s);
		else
			*dst++ = *s;
		}
	*dst = 0;
	return new SuString(buf, dst - buf);
	}

// methods ==========================================================

Value SuString::getdata(Value m)
	{
	int i;
	if (! m.int_if_num(&i))
		except("strings subscripts must be integers");
	return substr(i, 1);
	}

HashMap<Value,SuString::pmfn> SuString::methods;

#define METHOD(fn) methods[#fn] = &SuString::fn

Value SuString::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static bool first = true;
	if (first)
		{
		METHOD(Substr);
		METHOD(Size);
		METHOD(Asc);
		METHOD(Eval);
		METHOD(Eval2);
		METHOD(Compile);
		METHOD(Tr);
		METHOD(Find);
		METHOD(FindLast);
		METHOD(Find1of);
		METHOD(FindLast1of);
		METHOD(Findnot1of);
		METHOD(FindLastnot1of);
		METHOD(Match);
		METHOD(Extract);
		METHOD(Replace);
		METHOD(Split);
		METHOD(Entab);
		METHOD(Detab);
		METHOD(Repeat);
		METHOD(ServerEval);
		METHOD(Unescape);
		methods["Number?"] = &SuString::Numberq;
		METHOD(Iter);
		METHOD(Mbstowcs);
		METHOD(Wcstombs);
		METHOD(Upper);
		METHOD(Lower);
		methods["Upper?"] = &SuString::Upperq;
		methods["Lower?"] = &SuString::Lowerq;
		methods["Alpha?"] = &SuString::Alphaq;
		methods["Numeric?"] = &SuString::Numericq;
		methods["AlphaNum?"] = &SuString::AlphaNumq;
		first = false;
		}
	if (pmfn* p = methods.find(member))
		return (this->*(*p))(nargs, nargnames, argnames, each);
	else
		{
		static int G_SuStrings = globals("Strings");
		Value SuStrings = globals.find(G_SuStrings);
		SuObject* ob;
		if (SuStrings && (ob = SuStrings.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			unknown_method("string", member);
		}
	}

Value SuString::Substr(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs < 1 || nargs > 2)
		except("usage: string(i [,n])");
	int i = ARG(0).integer();
	if (i < 0)
		i += size();
	if (i < 0)
		i = 0;
	int n = nargs == 2 ? ARG(1).integer() : size();
	if (n < 0)
		n += size() - i;
	if (n < 0)
		n = 0;
	return substr(i, n);
	}

Value SuString::Size(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Size()");
	return size();
	}

Value SuString::Asc(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Asc()");
	return (uchar) *buf();
	}

bool isGlobal(char* s)
	{
	if (! isupper(*s))
		return false;
	while (*++s)
		if (*s != '_' && ! isalpha(*s) && ! isdigit(*s))
			return false;
	return true;
	}

Value SuString::Eval(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Eval()");
	if (isGlobal(str()))
		return globals[str()];
	if (Value result = run(str()))
		return result;
	else
		return Value(SuString::empty_string);
	}

Value SuString::Eval2(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Eval2()");
	Value result = isGlobal(str()) ? globals[str()] : 	run(str());
	SuObject* ob = new SuObject;
	if (result)
		ob->add(result);
	return ob;
	}

#include "compile.h"
#include "codecheck.h"
#include "codevisitor.h"

Value SuString::Compile(short nargs, short nargnames, ushort* argnames, int each)
	{
	CodeVisitor* visitor = 0;
	if (nargs == 1)
		visitor = make_codecheck(ARG(0).object());
	else if (nargs != 0)
		except("usage: string.Compile() or string.Compile(error_object)");
	return compile(str(), "", visitor);
	}

#include "dbms.h"

Value SuString::ServerEval(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.ServerEval()");
	return dbms()->run(str());
	}

#include "tr.h"

Value SuString::Tr(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1 && nargs != 2)
		except("usage: string.Tr(from [ , to ] )");
	char* to = nargs == 1 ? NULL : ARG(1).str();
	return new SuString(tr(str(), ARG(0).str(), to));
	}

Value SuString::Find(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.Find(string)");
	int i = gcstr().find(ARG(0).gcstr());
	return i == -1 ? size() : i;
	}

Value SuString::FindLast(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.FindLast(string)");
	char* buf = s.buf();
	char* str = ARG(0).str();
	int nstr = strlen(str);
	for (int i = size() - nstr; i >= 0; --i)
		if (0 == memcmp(buf + i, str, nstr))
				return i;
	return SuFalse;
	}

Value SuString::Find1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.Find1of(string)");
	char* buf = s.buf();
	int nbuf = size();
	char* set = ARG(0).str();
	for (int i = 0; i < nbuf; ++i)
		for (char* s = set; *s; ++s)
			if (buf[i] == *s)
				return i;
	return size();
	}

Value SuString::FindLast1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.FindLast1of(string)");
	char* buf = s.buf();
	char* set = ARG(0).str();
	for (int i = size() - 1; i >= 0; --i)
		for (char* s = set; *s; ++s)
			if (buf[i] == *s)
				return i;
	return SuFalse;
	}

Value SuString::Findnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.Findnot1of(string)");
	char* buf = s.buf();
	int nbuf = size();
	char* set = ARG(0).str();
	for (int i = 0; i < nbuf; ++i)
		for (char* s = set; ; ++s)
			if (! *s)
				return i;
			else if (buf[i] == *s)
				break ;
	return size();
	}

Value SuString::FindLastnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: string.FindLastnot1of(string)");
	char* buf = s.buf();
	char* set = ARG(0).str();
	for (int i = size() - 1; i >= 0; --i)
		for (char* s = set; ; ++s)
			if (! *s)
				return i;
			else if (buf[i] == *s)
				break ;
	return SuFalse;
	}

Value SuString::Match(short nargs, short nargnames, ushort* argnames, int each)
	{
	static ushort prev = ::symnum("prev");
	if (! (nargs == 1 || (nargs == 2 && nargnames == 1 && argnames[0] == prev)))
		except("usage: string.Match(pattern [, prev:] ) -> object");
	gcstring pat = ARG(0).gcstr();
	Rxpart parts[MAXPARTS];
	char* s = buf();
	int n = nargs == 1 ? size() : -size();
	if (! rx_match(buf(), n, rx_compile(pat), parts))
		return SuFalse;
	SuObject* ob = new SuObject;
	for (int i = 0; i < MAXPARTS; ++i)
		if (parts[i].n != -1)
			{
			SuObject* part = new SuObject;
			part->add(parts[i].s - s);
			part->add(parts[i].n);
			ob->add(part); //substr(parts[i].s - s, parts[i].n));
			}
	return ob;
	}

// "hello world".Extract(".....$") => "world"
// "hello world".Extract("(hello|howdy) (\w+)", 2) => "world"
Value SuString::Extract(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1 && nargs != 2)
		except("usage: string.Extract(pattern, part = 0/1) -> false or string");
	gcstring pat = ARG(0).gcstr();
	Rxpart parts[MAXPARTS];
	if (! rx_match(buf(), size(), rx_compile(pat), parts))
		return SuFalse;

	int i = nargs == 2 ? ARG(1).integer() : (parts[1].n >= 0 ? 1 : 0);
	return i < MAXPARTS && parts[i].n >= 0
		? new SuString(parts[i].n, parts[i].s) // no alloc constructor
		: SuString::empty_string;
	}

// "hello world".Replace("l", "L") => "heLLo worLd"
// "hello world".Replace(".", "<&>", 5) => "<h><e><l><l><o> world"
Value SuString::Replace(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs < 2 || 3 < nargs)
		except("usage: string.Replace(pattern, replacement, count = 9999) -> string");
	// pattern
	char* pat = rx_compile(ARG(0).gcstr());
	// replacement
	Value reparg = ARG(1);
	// TODO: replacement shouldn't have to be nul terminated
	const char* rep = reparg.str_if_str();
	if (! rep && (reparg.is_int() || 
		val_cast<SuNumber*>(reparg) || val_cast<SuBoolean*>(reparg)))
		rep = reparg.str();

	// count
	int count = (nargs < 3 ? 9999 : ARG(2).integer());

	int oldsize = size();
	int result_size = max(100, 2 * oldsize);
	char* result = new char[result_size];
	char* old = str();
	int lastm = -1;
	Rxpart parts[MAXPARTS];
	int dst = 0;
	for (int nsubs = 0, i = 0; i <= oldsize; )
		{
		int m;
		if (nsubs < count)
			m = rx_amatch(old, i, oldsize, pat, parts);
		else
			m = -1;
		if (m >= 0 && m != lastm)
			{
			if (rep)
				{
				int replen = rx_replen(rep, parts);
				while (dst + replen >= result_size)
					result = (char*) GC_realloc(result, result_size *= 2);
				rx_mkrep(result + dst, rep, parts);
				dst += replen;
				}
			else // block
				{
				KEEPSP
				gcstring match(parts[0].n, parts[0].s);
				PUSH(match);
				Value x = docall(reparg, CALL, 1, 0, 0, -1);
				gcstring replace = x ? x.gcstr() : match;
				while (dst + replace.size() >= result_size)
					result = (char*) GC_realloc(result, result_size *= 2);
				memcpy(result + dst, replace.buf(), replace.size());
				dst += replace.size();
				}
			++nsubs; 
			lastm = m;
			}
		if (m < 0 || m == i)
			{
			if (dst + 1 >= result_size)
				result = (char*) GC_realloc(result, result_size *= 2);
			result[dst++] = old[i++];
			}
		else
			i = m;
		}
	result[dst] = 0;
	return new SuString(result);
	}

Value SuString::Split(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1 || nargnames != 0)
		except("usage: string.Split(separator)");
	gcstring str = gcstr();
	gcstring sep = ARG(0).gcstr();
	if (sep == "")
		except("string.Split separator must not be empty string");
	SuObject* ob = new SuObject();
	int i = 0;
	for (int j; -1 != (j = str.find(sep, i)); i = j + sep.size())
		ob->add(new SuString(str.substr(i, j - i)));
	if (i < str.size())
		ob->add(new SuString(str.substr(i)));
	return ob;
	}

Value SuString::Detab(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Detab()");
	gcstring s = gcstr();
	int sn = s.size();

	const int MAXTABS = 32;
	short tabs[MAXTABS] = { 0 };
	// set custom tab stops here
	int nt = 1;
	const int w = 4;
	
	int ntabs = 0;
	for (int j = 0; j < sn; ++j)
		if (s[j] == '\t')
			++ntabs;
	const int n = sn + ntabs * (w - 1);
	char* buf = (char*) alloca(n);
	int i, si, col;
	for (si = 0, i = 0, col = 0; si < sn; ++si)
		{
		switch (s[si])
			{
		case '\t' :
			int j;
			for (j = 0; j < nt; ++j)
				if (col < tabs[j])
					break ;
			if (j >= nt)
				for (tabs[j] = tabs[j - 1]; col >= tabs[j]; tabs[j] += w)
					;
			while (col < tabs[j])
				buf[i++] = ' ', ++col;
			break ;			
		case '\n' :
		case '\r' :
			buf[i++] = s[si], col = 0;
			break ;
		default :
			buf[i++] = s[si], ++col;
			break ;
			}
		}
	verify(i <= n);
	verify(nt < MAXTABS);
	return new SuString(buf, i);
	}

inline bool istab(int col)
	{ return col > 0 && (col % 4) == 0; }

Value SuString::Entab(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Entab()");
	char* s = str();
	char* buf = (char*) alloca(size());
	char* dst = buf;
	char c;
	for (;;) // for each line
		{
		// convert leading spaces & tabs
		int col = 0;
		while (0 != (c = *s++))
			{
			if (c == ' ')
				++col;
			else if (c == '\t')
				for (++col; ! istab(col); ++col)
					;
			else
				break ;
			}
		--s;
		int dstcol = 0;
		for (int j = 0; j <= col; ++j)
			if (istab(j))
				{
				*dst++ = '\t';
				dstcol = j;
				}
		for (; dstcol < col; ++dstcol)
			*dst++ = ' ';

		// copy the rest of the line
		while ((c = *s++) && c != '\n' && c != '\r')
			*dst++ = c;

		// strip trailing spaces & tabs
		while (dst > buf && (dst[-1] == ' ' || dst[-1] == '\t'))
			--dst;
		if (! c)
			break ;
		*dst++ = c; // \n or \r
		}
	return new SuString(buf, dst - buf);
	}

Value SuString::Repeat(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1 || nargnames != 0)
		except("usage: string.Repeat(count)");
	char* src = buf();
	int len = size();
	int n = max(0, ARG(0).integer());
	SuString* t = new SuString(n * len);
	char* dst = t->buf();
	for (int i = 0; i < n; ++i, dst += len)
		memcpy(dst, src, len);
	return t;
	}

Value SuString::Numberq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Number?()");
	return size() == numlen(str()) ? SuBoolean::t : SuBoolean::f;
	}

Value SuString::Unescape(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Unescape()");

	int n = size();
	const char* s = buf();
	const char* lim = s + n;
	char* buf = (char*) alloca(n);
	char* dst = buf;
	for (; s < lim; ++s)
		{
		if (*s == '\\')
			*dst++ = doesc(s);
		else
			*dst++ = *s;
		}
	*dst = 0;
	return new SuString(buf, dst - buf);
	}

class SuStringIter : public SuValue
	{
public:
	explicit SuStringIter(gcstring s) : str(s), i(0)
		{ }
	virtual void out(Ostream& os)
		{ os << "StringIter"; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
private:
	gcstring str;
	int i;
	};

Value SuStringIter::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NEXT("Next");
	static Value ITER("Iter");

	if (member == NEXT)
		{
		if (nargs != 0)
			except("usage: stringiter.Next()");
		if (i >= str.size())
			return this;
		return new SuString(str.substr(i++, 1));
		}
	else if (member == ITER)
		{
		if (nargs != 0)
			except("usage: stringiter.Iter()");
		return this;
		}
	else
		except("method not found: " << member);
	}

Value SuString::Iter(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Iter()");
	return new SuStringIter(s);
	}

Value SuString::Mbstowcs(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Mbstowcs()");
	char* src = str();
	int n = mbstowcs(NULL, src, 0) + 1;
	SuString* dst = new SuString(n * 2);
	mbstowcs((wchar_t*) dst->buf(), src, n);
	return dst;
	}

Value SuString::Wcstombs(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Wcstombs()");
	wchar_t* src = (wchar_t*) str();
	int n = wcstombs(NULL, src, 0);
	SuString* dst = new SuString(n);
	wcstombs(dst->buf(), src, n);
	return dst;
	}

Value SuString::Upper(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Upper()");
	SuString* dst = new SuString(size());
	char* t = dst->buf();
/*#if defined(_MSC_VER) && _MSC_VER <= 1200
	const ctype<char>& ct = use_facet< ctype<char> >(locale(), 0, true);
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = ct.toupper((unsigned int) *s);
#else*/
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = toupper((unsigned int) *s);
//#endif
	return dst;
	}

Value SuString::Lower(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Lower()");
	SuString* dst = new SuString(size());
	char* t = dst->buf();
/*#if defined(_MSC_VER) && _MSC_VER <= 1200
	const ctype<char>& ct = use_facet< ctype<char> >(locale(), 0, true);
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = ct.tolower((unsigned int) *s);
#else*/
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = tolower((unsigned int) *s);
//#endif
	return dst;
	}

Value SuString::Upperq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Upper?()");
	bool result = false;
	for (char* s = begin(); s != end(); ++s)
		if (islower((unsigned int) *s))
			return SuFalse;
		else if (isupper((unsigned int) *s))
			result = true;
	return result ? SuTrue : SuFalse;
	}

Value SuString::Lowerq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Lower?()");
	bool result = false;
	for (char* s = begin(); s != end(); ++s)
		if (isupper((unsigned int) *s))
			return SuFalse;
		else if (islower((unsigned int) *s))
			result = true;
	return result ? SuTrue : SuFalse;
	}

Value SuString::Alphaq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Alpha?()");
	if (size() <= 0)
		return SuFalse;
	for (char* s = begin(); s != end(); ++s)
		if (! isalpha((unsigned int) *s))
			return SuFalse;
	return SuTrue;
	}

Value SuString::Numericq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Numericq?()");
	if (size() <= 0)
		return SuFalse;
	for (char* s = begin(); s != end(); ++s)
		if (! isdigit((unsigned int) *s))
			return SuFalse;
	return SuTrue;
	}

Value SuString::AlphaNumq(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.AlphaNum?()");
	if (size() <= 0)
		return SuFalse;
	for (char* s = begin(); s != end(); ++s)
		if (! isalnum((unsigned int) *s))
			return SuFalse;
	return SuTrue;
	}

// tests ============================================================

#include "testing.h"

class test_sustring : public Tests
	{
	TEST(0, main)
		{
		// TODO
		}
	TEST(1, extract)
		{
		verify(SuTrue == run("'hello world'.Extract('.....$') is 'world'"));
		verify(SuTrue == run("'hello world'.Extract('w(..)ld') is 'or'"));
		}
	TEST(2, replace)
		{
		verify(SuTrue == run("'hello world'.Replace('l', 'L') == 'heLLo worLd'"));
		verify(SuTrue == run("'hello world'.Replace('.', '<&>', 5) == '<h><e><l><l><o> world'"));
		verify(SuTrue == run("'world'.Replace('^', 'hello ') == 'hello world'"));
		verify(SuTrue == run("'hello'.Replace('$', ' world') == 'hello world'"));
		}
	TEST(3, match)
		{
		verify(SuTrue == run("'hello world'.Match('w(..)ld') is #((6,5),(7,2))"));
		}
	TEST(4, is_identifier)
		{
		verify(! is_identifier(""));
		verify(! is_identifier("?"));
		verify(! is_identifier("a b"));
		verify(! is_identifier("123"));
		verify(! is_identifier("?ab"));
		verify(! is_identifier("a?b"));
		verify(is_identifier("x"));
		verify(is_identifier("xyz"));
		verify(is_identifier("Abc"));
		verify(is_identifier("_x"));
		verify(is_identifier("x_"));
		verify(is_identifier("x_1"));
		verify(is_identifier("x?"));
		}
	};
REGISTER(test_sustring);
