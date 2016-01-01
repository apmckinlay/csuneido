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
#include "symbols.h"
#include "minmax.h"
#include "range.h"
#include "buffer.h"
#include "func.h" // for argseach for call
#include "scanner.h" // for doesc

SuString* SuString::empty_string = new SuString("");

void SuString::out(Ostream& out)
	{
	extern bool obout_inkey;
	if (obout_inkey && is_identifier())
		out << s;
	else if (backquote())
		out << '`' << s << '`';
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

bool SuString::backquote() const
	{
	if (-1 != s.find('`') || -1 == s.find('\\'))
		return false;
	for (const char* t = s.begin(); t != s.end(); ++t)
		if (! isprint(*t))
			return false;
	return true;
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

SuBuffer::SuBuffer(size_t n, const gcstring& s) : SuString(n)
	{
	size_t sn = s.size();
	if (n < sn)
		except("Buffer must be large enough for initial string");
	memcpy(buf(), s.buf(), sn);
	memset(buf() + sn, 0, n - sn);
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

// methods ==========================================================

Value SuString::getdata(Value m)
	{
	if (Range* r = val_cast<Range*>(m))
		return new SuString(r->substr(gcstr()));
	int i;
	if (! m.int_if_num(&i))
		except("string subscripts must be integers");
	if (i < 0)
		i += size();
	return substr(i, 1);
	}

HashMap<Value,SuString::pmfn> SuString::methods;

#define METHOD(fn) methods[#fn] = &SuString::fn

Value SuString::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
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
		METHOD(Iter);
		METHOD(Lower);
		methods["Lower?"] = &SuString::Lowerq;
		METHOD(MapN);
		METHOD(Match);
		METHOD(Mbstowcs);
		methods["Number?"] = &SuString::Numberq;
		methods["Numeric?"] = &SuString::Numericq;
		methods["Prefix?"] = &SuString::Prefixq;
		METHOD(Repeat);
		METHOD(Replace);
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
	else
		{
		static int G_SuStrings = globals("Strings");
		Value SuStrings = globals.find(G_SuStrings);
		SuObject* ob;
		if (SuStrings && (ob = SuStrings.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			method_not_found("string", member);
		}
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
	gcstring to = nargs == 1 ? "" : ARG(1).gcstr();
	return new SuString(tr(gcstr(), ARG(0).gcstr(), to));
	}

Value SuString::Find(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Find(string, pos = 0)");
	gcstring str = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();
	
	int i = s.find(str, pos);
	return i == -1 ? size() : i;
	}

Value SuString::FindLast(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.FindLast(string, pos = size())");
	char* str = args.getstr("string");
	int pos = args.getint("pos", size());
	args.end();
	
	int i = s.findlast(str, pos);
	return i == -1 ? SuFalse : i;
	}

Value SuString::Find1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Find1of(string, pos = 0)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();

	char* buf = s.buf();
	int nbuf = size();
	char* setbuf = set.buf();
	char* setlim = setbuf + set.size();
	for (int i = max(pos, 0); i < nbuf; ++i)
		for (char* s = setbuf; s < setlim; ++s)
			if (buf[i] == *s)
				return i;
	return size();
	}

Value SuString::FindLast1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.FindLast1of(string, pos = size() - 1)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", size() - 1);
	args.end();
	
	char* buf = s.buf();
	char* setbuf = set.buf();
	char* setlim = setbuf + set.size();
	for (int i = min(pos, size() - 1); i >= 0; --i)
		for (char* s = setbuf; s < setlim; ++s)
			if (buf[i] == *s)
				return i;
	return SuFalse;
	}

Value SuString::Findnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Findnot1of(string, pos = 0)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();

	char* buf = s.buf();
	int nbuf = size();
	char* setbuf = set.buf();
	char* setlim = setbuf + set.size();
	for (int i = max(pos, 0); i < nbuf; ++i)
		for (char* s = setbuf; ; ++s)
			if (s >= setlim)
				return i;
			else if (buf[i] == *s)
				break ;
	return size();
	}

Value SuString::FindLastnot1of(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.FindLastnot1of(string, pos = size() - 1)");
	gcstring set = args.getgcstr("string");
	int pos = args.getint("pos", size() - 1);
	args.end();
	
	char* buf = s.buf();
	char* setbuf = set.buf();
	char* setlim = setbuf + set.size();
	for (int i = min(pos, size() - 1); i >= 0; --i)
		for (char* s = setbuf; ; ++s)
			if (s >= setlim)
				return i;
			else if (buf[i] == *s)
				break ;
	return SuFalse;
	}

Value SuString::Match(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Match(pattern, pos = false, prev = false) => object");
	gcstring pat = args.getgcstr("pattern");
	Value posval = args.getValue("pos", SuFalse);
	bool prev = args.getValue("prev", SuFalse) == SuTrue;
	int pos = (posval == SuFalse) ? (prev ? size() : 0) : posval.integer();
	args.end();
	Rxpart parts[MAXPARTS];
	char* s = buf();
	bool hasmatch = prev ? rx_match_reverse(s, size(), pos, rx_compile(pat), parts)
						 : rx_match(s, size(), pos, rx_compile(pat), parts);
	if (! hasmatch)
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
	if (! rx_match(buf(), size(), 0, rx_compile(pat), parts))
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
	if (nargs < 1 || 3 < nargs)
		except("usage: string.Replace(pattern, replacement = '', count = false) -> string");
	// pattern
	char* pat = rx_compile(ARG(0).gcstr());
	// replacement
	Value reparg = nargs == 1 ? SuEmptyString : ARG(1);
	// TODO: replacement shouldn't have to be nul terminated
	const char* rep = reparg.str_if_str();
	if (! rep && (reparg.is_int() || 
		val_cast<SuNumber*>(reparg) || val_cast<SuBoolean*>(reparg)))
		rep = reparg.str();

	// count
	int count = (nargs < 3 ? INT_MAX : ARG(2).integer());

	int oldsize = size();
	Buffer result(oldsize + oldsize / 4); // usually result will be similar size
	char* old = str();
	int lastm = -1;
	Rxpart parts[MAXPARTS];
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
				rx_mkrep(result.alloc(replen), rep, parts);
				}
			else // block
				{
				KEEPSP
				gcstring match(parts[0].n, parts[0].s);
				PUSH(new SuString(match));
				Value x = docall(reparg, CALL, 1, 0, 0, -1);
				gcstring replace = x ? x.gcstr() : match;
				result.add(replace.buf(), replace.size());
				}
			++nsubs; 
			lastm = m;
			}
		if (m < 0 || m == i)
			{
			if (i == oldsize)
				break;
			result.add(old[i++]);
			}
		else
			i = m;
		}
	return new SuString(result.gcstr());
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
		dst.add(docall(block, CALL, 1, 0, 0, -1).gcstr());
		}
	return new SuString(dst.gcstr());
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
	return size() == numlen(str()) ? SuTrue : SuFalse;
	}

Value SuString::Unescape(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Unescape()");

	int n = size();
	const char* s = buf();
	char* buf = (char*) alloca(n);
	char* dst = buf;
	for (int i = 0; i < n; ++i)
		{
		if (s[i] == '\\')
			*dst++ = Scanner::doesc(s, i);
		else
			*dst++ = s[i];
		}
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
		method_not_found(type(), member);
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
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = toupper((unsigned int) *s);
	return dst;
	}

Value SuString::Lower(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: string.Lower()");
	SuString* dst = new SuString(size());
	char* t = dst->buf();
	for (char* s = begin(); s != end(); ++s, ++t)
		*t = tolower((unsigned int) *s);
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
		except("usage: string.Numeric?()");
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

Value SuString::Hasq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Has?(string)");
	gcstring str = args.getgcstr("string");
	args.end();
	return s.find(str) == -1 ? SuFalse : SuTrue;
	}

Value SuString::Prefixq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Prefix?(string, pos = 0)");
	gcstring str = args.getgcstr("string");
	int pos = args.getint("pos", 0);
	args.end();
	return s.has_prefix(str, pos) ? SuTrue : SuFalse;
	}

Value SuString::Suffixq(short nargs, short nargnames, ushort* argnames, int each)
	{
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("usage: string.Suffix?(string)");
	gcstring str = args.getgcstr("string");
	args.end();
	return s.has_suffix(str) ? SuTrue : SuFalse;
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

class test_sustring2 : public Tests
	{
	TEST(0, main)
		{
		Value result = run("'a\\x00bc'.Replace('[\\x00-\\xff][\\x00-\\xff]?[\\x00-\\xff]?', {|x| x.Size() })");
//cout << "value result: " << result << endl;
char buf[] = { '3', '1', 0 }; //'a', 0, 'b', 'c', 0 };
gcstring g(sizeof (buf) - 1, buf);
SuString s(g);
Value v(&s);
//cout << "should be: " << v << endl;
		assert_eq(result, v);
		}
	};
REGISTER(test_sustring2);
