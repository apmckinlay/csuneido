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
#include "builtinargs.h"
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
#include "range.h"
#include "buffer.h"
#include "func.h" // for argseach for call
#include "scanner.h" // for doesc
#include <algorithm>
#include "htbl.h"
using std::min;
using std::max;

static_assert(sizeof(SuString) == 12);

extern bool obout_inkey;

void SuString::out(Ostream& out) const
	{
	if (obout_inkey && is_identifier())
		out << s;
	else if (backquote())
		out << '`' << s << '`';
	else
		{
		char quote = (s.find('"') != -1 && s.find('\'') == -1) ? '\'' : '"';
		out << quote;
		for (auto c : s)
			{
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
	return ::is_identifier(s.ptr(), s.size());
	}

bool SuString::backquote() const
	{
	if (-1 != s.find('`') || -1 == s.find('\\'))
		return false;
	for (auto c : s)
		if (! isprint(c))
			return false;
	return true;
	}

int SuString::symnum() const
	{
	return ::symnum(str());
	}

int SuString::integer() const
	{
	if (size() == 0)
		return 0; // "" => 0
	except("can't convert String to number");
	}

SuNumber* SuString::number()
	{
	if (size() == 0)
		return &SuNumber::zero; // "" => 0
	except("can't convert String to number");
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

SuBuffer::SuBuffer(size_t n, const gcstring& init) : SuString(n)
	{
	size_t sn = init.size();
	if (n < sn)
		except("Buffer must be large enough for initial string");
	memcpy(s.buf(), init.ptr(), sn);
	memset(s.buf() + sn, 0, n - sn);
	//TODO don't use buf()
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
	memcpy(dst + 1, ptr(), n);
	}

/*static*/ Value SuString::unpack(const gcstring& s)
	{
	if (s.size() <= 1)
		return SuEmptyString;
	else
		return new SuString(s.substr(1));
	}

// methods ==========================================================

Value SuString::getdata(Value m)
	{
	int i;
	if (! m.int_if_num(&i))
		except("string subscripts must be integers");
	if (i < 0)
		i += size();
	return substr(i, 1);
	}

Value SuString::rangeTo(int i, int j)
	{
	int size = this->size();
	int f = prepFrom(i, size);
	int t = prepTo(j, size);
	if (t <= f)
		return "";
	return substr(f, t - f);
	}

Value SuString::rangeLen(int i, int n)
	{
	int f = prepFrom(i, size());
	return substr(f, n);
	}

typedef Value(SuString::*pmfn)(short, short, ushort*, int);

Hmap<Value,pmfn> methods;

#define METHOD(fn) methods[#fn] = &SuString::fn

Value SuString::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static bool first = true;
	if (first)
		{
		first = false;
		METHOD(Asc);
		methods["Alpha?"] = &SuString::Alphaq;
		methods["AlphaNum?"] = &SuString::AlphaNumq;
		methods[CALL] = &SuString::Call;
		METHOD(Compile);
		METHOD(CountChar);
		METHOD(Detab);
		METHOD(Entab);
		METHOD(Eval);
		METHOD(Eval2);
		METHOD(Extract);
		METHOD(Find);
		METHOD(FindLast);
		METHOD(Find1of);
		METHOD(FindLast1of);
		METHOD(Findnot1of);
		METHOD(FindLastnot1of);
		methods["Has?"] = &SuString::Hasq;
		METHOD(Instantiate);
		METHOD(Iter);
		METHOD(Lower);
		methods["Lower?"] = &SuString::Lowerq;
		METHOD(MapN);
		METHOD(Match);
		METHOD(Mbstowcs);
		METHOD(NthLine);
		methods["Number?"] = &SuString::Numberq;
		methods["Numeric?"] = &SuString::Numericq;
		methods["Prefix?"] = &SuString::Prefixq;
		METHOD(Repeat);
		METHOD(Replace);
		METHOD(Reverse);
		METHOD(ServerEval);
		METHOD(Size);
		METHOD(Split);
		METHOD(Substr);
		methods["Suffix?"] = &SuString::Suffixq;
		METHOD(Tr);
		METHOD(Unescape);
		METHOD(Upper);
		methods["Upper?"] = &SuString::Upperq;
		METHOD(Wcstombs);
		}
	if (pmfn* p = methods.find(member))
		return (this->*(*p))(nargs, nargnames, argnames, each);
	static UserDefinedMethods udm("Strings");
	if (Value c = udm(member))
		return c.call(self, member, nargs, nargnames, argnames, each);
	method_not_found("string", member);
	}

Value SuString::Call(short nargs, short nargnames, ushort* argnames, int each)
	{
	// #symbol(ob, ...) => ob.symbol(...)
	if (nargs < 1)
		except("string call requires 'this' argument");
	// remove first argument (object) from stack
	argseach(nargs, nargnames, argnames, each); // have to expand first
	Value* args = GETSP() - nargs + 1;
	Value ob = args[0];
	for (int i = 1; i < nargs; ++i)
		args[i - 1] = args[i];
	POP();
	return ob.call(ob, this, nargs - 1, nargnames, argnames, each);
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
	NOARGS("string.Size()");
	return size();
	}

Value SuString::Asc(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Asc()");
	return (uchar) *ptr();
	}

bool isGlobal(const char* s)
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
	NOARGS("string.Eval()");
	if (isGlobal(str()))
		return globals[str()];
	if (Value result = run(str()))
		return result;
	else
		return SuEmptyString;
	}

Value SuString::Eval2(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Eval2()");
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
	NOARGS("string.ServerEval()");
	return dbms()->run(str());
	}

#include "tr.h"

Value SuString::Tr(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1 && nargs != 2)
		except("usage: string.Tr(from [ , to ] )");
	gcstring to = nargs == 1 ? "" : ARG(1).gcstr();
	return new SuString(tr(gcstr(), ARG(0).gcstr(), to));
	}

Value SuString::Find(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Find(string, pos = 0)");
	gcstring str = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();

	int i = s.find(str, pos);
	return i == -1 ? size() : i;
	}

Value SuString::FindLast(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.FindLast(string, pos = size())");
	auto str = args.getstr("string");
	int pos = args.getint("pos", size());
	args.end();

	int i = s.findlast(str, pos);
	return i == -1 ? SuFalse : i;
	}

Value SuString::Find1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Find1of(string, pos = 0)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();

	auto buf = s.ptr();
	int nbuf = size();
	for (int i = max(pos, 0); i < nbuf; ++i)
		for (auto c : set)
			if (buf[i] == c)
				return i;
	return size();
	}

Value SuString::FindLast1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.FindLast1of(string, pos = size() - 1)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", size() - 1);
	args.end();

	auto buf = s.ptr();
	for (int i = min(pos, size() - 1); i >= 0; --i)
		for (auto c : set)
			if (buf[i] == c)
				return i;
	return SuFalse;
	}

Value SuString::Findnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Findnot1of(string, pos = 0)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();

	auto buf = s.ptr();
	int nbuf = size();
	auto setbuf = set.ptr();
	auto setlim = setbuf + set.size();
	for (int i = max(pos, 0); i < nbuf; ++i)
		for (auto t = setbuf; ; ++t)
			if (t >= setlim)
				return i;
			else if (buf[i] == *t)
				break ;
	return size();
	}

Value SuString::FindLastnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.FindLastnot1of(string, pos = size() - 1)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", size() - 1);
	args.end();

	auto buf = s.ptr();
	auto setbuf = set.ptr();
	auto setlim = setbuf + set.size();
	for (int i = min(pos, size() - 1); i >= 0; --i)
		for (auto t = setbuf; ; ++t)
			if (t >= setlim)
				return i;
			else if (buf[i] == *t)
				break ;
	return SuFalse;
	}

Value SuString::Match(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Match(pattern, pos = false, prev = false) => object");
	gcstring pat = args.getgcstr("pattern");
	Value posval = args.getValue("pos", SuFalse);
	bool prev = args.getValue("prev", SuFalse).toBool();
	int pos = (posval == SuFalse) ? (prev ? size() : 0) : posval.integer();
	args.end();
	Rxpart parts[MAXPARTS];
	const char* t = ptr();
	bool hasmatch = prev ? rx_match_reverse(t, size(), pos, rx_compile(pat), parts)
						 : rx_match(t, size(), pos, rx_compile(pat), parts);
	if (! hasmatch)
		return SuFalse;
	SuObject* ob = new SuObject;
	for (int i = 0; i < MAXPARTS; ++i)
		if (parts[i].n != -1)
			{
			SuObject* part = new SuObject;
			part->add(parts[i].s - t);
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
	if (! rx_match(ptr(), size(), 0, rx_compile(pat), parts))
		return SuFalse;

	int i = nargs == 2 ? ARG(1).integer() : (parts[1].n >= 0 ? 1 : 0);
	return i < MAXPARTS && parts[i].n >= 0
		? SuString::noalloc(parts[i].s, parts[i].n)
		: SuEmptyString;
	}

// "hello world".Replace("l", "L") => "heLLo worLd"
// "hello world".Replace(".", "<&>", 5) => "<h><e><l><l><o> world"
Value SuString::Replace(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs < 1 || 3 < nargs)
		except("usage: string.Replace(pattern, replacement = '', count = false) -> string");
	// replacement
	Value reparg = nargs == 1 ? SuEmptyString : ARG(1);
	// count
	int count = (nargs < 3 ? INT_MAX : ARG(2).integer());
	return replace(ARG(0).gcstr(), reparg, count);
	}

Value SuString::replace(const gcstring& patarg, Value reparg, int count) const
	{
	if (count <= 0)
		return this;
	const char* pat = rx_compile(patarg);
	gcstring rep;
	if (val_cast<SuString*>(reparg) || val_cast<SuBoolean*>(reparg) || 
		reparg.is_int() || val_cast<SuNumber*>(reparg))
		{ rep = reparg.gcstr(); reparg = Value(); }
	int oldsize = size();
	Buffer* result = nullptr; // construct only if needed
	auto old = ptr();
	int from = 0; // where to copy from the original string next
	int lastm = -1;
	Rxpart parts[MAXPARTS];
	for (int nsubs = 0, i = 0; i <= oldsize; )
		{
		int m = rx_amatch(old, i, oldsize, pat, parts);
		if (m >= 0 && m != lastm)
			{
			if (!result)
				result = new Buffer(oldsize + oldsize / 4 + 1);
			result->add(s.substr(from, i - from));
			if (!reparg)
				{
				int replen = rx_replen(rep, parts);
				rx_mkrep(result->alloc(replen), rep, parts);
				}
			else // block
				{
				KEEPSP
				gcstring match = gcstring::noalloc(parts[0].s, parts[0].n);
				PUSH(new SuString(match));
				Value x = docall(reparg, CALL, 1);
				gcstring replace = x ? x.gcstr() : match;
				result->add(replace.ptr(), replace.size());
				}
			lastm = from = m;
			if (++nsubs >= count)
				break;
			}
		if (m < 0 || m == i)
			++i;
		else
			i = m;
		}
	if (!result) // no replacements
		return this;
	result->add(s.substr(from));
	return new SuString(result->gcstr());
	}

Value SuString::MapN(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 2)
		except("usage: string.MapN(n, block)");
	int n = ARG(0).integer();
	Value block = ARG(1);
	Buffer dst; // don't allocate large buffer because MapN may be used just for side effects
	for (int i = 0; i < size(); i += n)
		{
		KEEPSP
		PUSH(new SuString(s.substr(i, n)));
		if (auto value = docall(block, CALL, 1))
			dst.add(value.gcstr());
		}
	return new SuString(dst.gcstr());
	}

Value SuString::NthLine(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.NthLine(n)");
	int n = args.getint("n");
	args.end();
	const char* p = ptr();
	const char* lim = p + size();
	for (; p < lim && n > 0; ++p)
		if (*p == '\n')
			--n;
	const char* t = p;
	while (t < lim && *t != '\n')
		++t;
	while (t > p && t[-1] == '\r')
		--t;
	return new SuString(gcstring::noalloc(p, t - p));
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

Value SuString::CountChar(short nargs, short nargnames, ushort* argnames, 
	int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.CountChar(c)");
	gcstring sc = args.getgcstr("c");
	args.end();
	if (sc.size() != 1)
		args.exceptUsage();
	char c = sc[0];
	gcstring str = gcstr();
	return std::count_if(str.begin(), str.end(), 
		[c](char sch) { return sch == c; });
	}

Value SuString::Detab(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Detab()");
	const char* ss = ptr();
	int sn = size();

	const int MAXTABS = 32;
	short tabs[MAXTABS] = { 0 };
	// set custom tab stops here
	int nt = 1;
	const int w = 4;

	int ntabs = 0;
	for (int j = 0; j < sn; ++j)
		if (ss[j] == '\t')
			++ntabs;
	const int n = sn + ntabs * (w - 1);
	char* buf = (char*) _alloca(n);
	int i, si, col;
	for (si = 0, i = 0, col = 0; si < sn; ++si)
		{
		switch (ss[si])
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
			buf[i++] = ss[si], col = 0;
			break ;
		default :
			buf[i++] = ss[si], ++col;
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
	NOARGS("string.Entab()");
	auto ss = str();
	char* buf = (char*) _alloca(size());
	char* dst = buf;
	char c;
	for (;;) // for each line
		{
		// convert leading spaces & tabs
		int col = 0;
		while (0 != (c = *ss++))
			{
			if (c == ' ')
				++col;
			else if (c == '\t')
				for (++col; ! istab(col); ++col)
					;
			else
				break ;
			}
		--ss;
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
		while (0 != (c = *ss++) && c != '\n' && c != '\r')
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
	int len = size();
	int n = max(0, ARG(0).integer());
	char* buf = salloc(n * len);
	char* dst = buf;
	for (int i = 0; i < n; ++i, dst += len)
		memcpy(dst, ptr(), len);
	return SuString::noalloc(buf, n * len);
	}

Value SuString::Numberq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Number?()");
	return size() == numlen(str()) ? SuTrue : SuFalse;
	}

Value SuString::Unescape(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Unescape()");

	int n = size();
	const char* ss = ptr();
	char* buf = (char*) _alloca(n);
	char* dst = buf;
	for (int i = 0; i < n; ++i)
		{
		if (ss[i] == '\\')
			*dst++ = Scanner::doesc(ss, i);
		else
			*dst++ = ss[i];
		}
	return new SuString(buf, dst - buf);
	}

class SuStringIter : public SuValue
	{
public:
	explicit SuStringIter(gcstring s) : str(s), i(0)
		{ }

	void out(Ostream& os) const override
		{ os << "StringIter"; }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
private:
	gcstring str;
	int i;
	};

Value SuStringIter::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NEXT("Next");
	static Value ITER("Iter");

	if (member == NEXT)
		{
		if (nargs > nargnames)
			except("usage: stringiter.Next()");
		if (i >= str.size())
			return this;
		return new SuString(str.substr(i++, 1));
		}
	else if (member == ITER)
		{
		if (nargs > nargnames)
			except("usage: stringiter.Iter()");
		return this;
		}
	else
		method_not_found(type(), member);
	}

Value SuString::Iter(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Iter()");
	return new SuStringIter(s);
	}

Value SuString::Mbstowcs(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Mbstowcs()");
	auto src = str();
	int n = mbstowcs(NULL, src, 0) + 1;
	char* buf = salloc(n * 2);
	mbstowcs((wchar_t*) buf, src, n);
	return SuString::noalloc(buf, n * 2);
	}

Value SuString::Wcstombs(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Wcstombs()");
	wchar_t* src = (wchar_t*) str();
	int n = wcstombs(NULL, src, 0);
	char* buf = salloc(n);
	wcstombs(buf, src, n);
	return SuString::noalloc(buf, n);
	}

Value SuString::Upper(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Upper()");
	char* buf = salloc(size());
	char* dst = buf;
	for (auto c : s)
		*dst++ = toupper((unsigned int) c);
	return SuString::noalloc(buf, size());
	}

Value SuString::Lower(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Lower()");
	return tolower();
	}

SuString* SuString::tolower() const
	{
	char* buf = salloc(size());
	char* dst = buf;
	for (auto c : s)
		*dst++ = ::tolower((unsigned int)c);
	return SuString::noalloc(buf, size());
	}

Value SuString::Upperq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Upper?()");
	bool result = false;
	for (auto c : s)
		if (islower((unsigned int) c))
			return SuFalse;
		else if (isupper((unsigned int) c))
			result = true;
	return result ? SuTrue : SuFalse;
	}

Value SuString::Lowerq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Lower?()");
	bool result = false;
	for (auto c : s)
		if (isupper((unsigned int) c))
			return SuFalse;
		else if (islower((unsigned int) c))
			result = true;
	return result ? SuTrue : SuFalse;
	}

Value SuString::Alphaq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Alpha?()");
	if (size() <= 0)
		return SuFalse;
	for (auto c : s)
		if (! isalpha((unsigned int) c))
			return SuFalse;
	return SuTrue;
	}

Value SuString::Numericq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Numeric?()");
	if (size() <= 0)
		return SuFalse;
	for (auto c : s)
		if (! isdigit((unsigned int) c))
			return SuFalse;
	return SuTrue;
	}

Value SuString::AlphaNumq(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.AlphaNum?()");
	if (size() <= 0)
		return SuFalse;
	for (auto c : s)
		if (! isalnum((unsigned int) c))
			return SuFalse;
	return SuTrue;
	}

Value SuString::Hasq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Has?(string)");
	gcstring str = args.getgcstr("string");
	args.end();
	return s.find(str) == -1 ? SuFalse : SuTrue;
	}

Value SuString::Prefixq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Prefix?(string, pos = 0)");
	gcstring str = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();
	if (pos < 0)
		pos += s.size();
	if (pos < 0)
		pos = 0;
	return s.has_prefix(str, pos) ? SuTrue : SuFalse;
	}

Value SuString::Suffixq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("string.Suffix?(string)");
	gcstring str = args.getgcstr("string");
	args.end();
	return s.has_suffix(str) ? SuTrue : SuFalse;
	}

Value SuString::Instantiate(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Instantiate()");
	s.instantiate();
	return this;
	}

Value SuString::Reverse(short nargs, short nargnames, ushort* argnames, int each)
	{
	NOARGS("string.Reverse()");
	char* buf = salloc(size());
	char* dst = buf + size();
	*dst = 0;
	for (auto c : s)
		*--dst = c;
	verify(dst == buf);
	return SuString::noalloc(buf, size());
	}

// tests ------------------------------------------------------------

#include "testing.h"

TEST(sustring_extract)
	{
	verify(SuTrue == run("'hello world'.Extract('.....$') is 'world'"));
	verify(SuTrue == run("'hello world'.Extract('w(..)ld') is 'or'"));
	}

TEST(sustring_replace)
	{
	assert_eq(SuString("hello world").replace("x", "X"), "hello world"); // same
	assert_eq(SuString("hello world").replace("l", "L"), "heLLo worLd");
	assert_eq(SuString("hello world").replace("l", "L", 0), "hello world"); // same
	assert_eq(SuString("hello world").replace(".", "<&>", 5), "<h><e><l><l><o> world");
	assert_eq(SuString("world").replace("^", "hello "), "hello world");
	assert_eq(SuString("hello").replace("$", " world"), "hello world");
	}

TEST(sustring_match)
	{
	verify(SuTrue == run("'hello world'.Match('w(..)ld') is #((6,5),(7,2))"));
	}

TEST(sustring_is_identifier)
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
