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

#include "regexp.h"
#include <limits.h>
#include "cachemap.h"
#include "gcstring.h"
#include "except.h"
#include "charmatcher.h"
#include <typeinfo>
#include <vector>

typedef std::vector<int> IntList;

/*
* regular expression grammar and compiled form:
*
*	regex	:	sequence				LEFT0 ... RIGHT0
*			|	sequence (| sequence)+	Branch sequence (Jump Branch sequence)+
*
*	sequence	:	element+
*
*	element		:	^					startOfLine
*				|	$					endOfLine
*				|	\A					startOfString
*				|	\Z					endOfString
*				|	(?i)					(only affects compile)
*				|	(?-i)				(only affects compile)
*				|	(?q)					(only affects compile)
*				|	(?-q)				(only affects compile)
*				|	\<					startOfWord
*				|	\>					endOfWord
*				|	\#					Backref(#)
*				|	simple
*				|	simple ?				Branch simple
*				|	simple +				simple Branch
*				|	simple *				Branch simple Branch
*				|	simple ??			Branch simple
*				|	simple +?			simple Branch
*				|	simple *?			Branch simple Branch
*
*	simple		:	.					any
*				|	[ charmatch+ ]		CharClass
*				|	[^ charmatch+ ]		CharClass
*				|	shortcut				CharClass
*				|	( regex )			Left(i) ... Right(i)
*				|	chars				Chars(string) // multiple characters
*
*	charmatch	:	shortcut				CharClass
*				|	posix				CharClass
*				|	char - char			CharClass
*				|	char					CharClass
*
*	shortcut	:	\d						CharClass
*				|	\D					CharClass
*				|	\w					CharClass
*				|	\W					CharClass
*				|	\s					CharClass
*				|	\S					CharClass
*
*	posix		|	[:alnum:]			CharClass
*				|	[:alpha:]			CharClass
*				|	[:blank:]			CharClass
*				|	[:cntrl:]			CharClass
*				|	[:digit:]			CharClass
*				|	[:graph:]			CharClass
*				|	[:lower:]			CharClass
*				|	[:print:]			CharClass
*				|	[:punct:]			CharClass
*				|	[:space:]			CharClass
*				|	[:upper:]			CharClass
*				|	[:xdigit:]			CharClass
*
* handling ignore case:
* - case is ASCII only i.e. a-z, A-Z, not unicode
* - compile Chars to lower case, match has to convert to lower case
* - CharClass tries matching both toupper and tolower (to handle ranges)
* - Backref converts both to lower
*
* Element.nextPossible is used to optimize match
* if amatch fails at a certain position
* nextPossible skips ahead
* so it doesn't just try amatch at every position
* This makes match almost as fast as indexOf or contains
*/

class Element
	{
public:
	virtual ~Element() = default;

	virtual int omatch(const char* s, int si, int sn, Rxpart* res) const
		{
		return omatch(s, si, sn);
		}
	virtual int omatch(const char* s, int si, int sn) const
		{
		return -1;
		};
	virtual int nextPossible(const char* s, int si, int sn) const
		{
		return si + 1;
		}
	};

// compile
class RxCompile
	{
public:
	RxCompile(const char* src, int len) : src(src), sn(len)
		{ }
	char* compile();

	static CharMatcher* blank;
	static CharMatcher* digit;
	static CharMatcher* notDigit;
	static CharMatcher* lower;
	static CharMatcher* upper;
	static CharMatcher* alpha;
	static CharMatcher* alnum;
	static CharMatcher* punct;
	static CharMatcher* graph;
	static CharMatcher* print;
	static CharMatcher* xdigit;
	static CharMatcher* space;
	static CharMatcher* notSpace;
	static CharMatcher* cntrl;
	static CharMatcher* word;
	static CharMatcher* notWord;
private:
	void regexp();
	void sequence();
	void element();
	void quoted();
	void simple();
	void charClass();
	CharMatcher* posixClass();

	void emit(Element* e);
	void emitChars(const char* s, int n);
	void insert(int i, Element* e);
	bool match(const char* s);
	bool match(char c);
	void mustMatch(char c);
	bool matchBackref();
	bool matchRange();
	bool next1of(const char* const set) const;

	const char* src;
	int si = 0;
	int sn;
	bool ignoringCase = false;
	int leftCount = 0;
	bool inChars = false;
	bool inCharsIgnoringCase = false;
	std::vector<Element*> pat;
	};

CharMatcher* RxCompile::blank = CharMatcher::anyOf(" \t");
CharMatcher* RxCompile::digit = CharMatcher::anyOf("0123456789");
CharMatcher* RxCompile::notDigit = RxCompile::digit->negate();
CharMatcher* RxCompile::lower = CharMatcher::inRange('a', 'z');
CharMatcher* RxCompile::upper = CharMatcher::inRange('A', 'Z');
CharMatcher* RxCompile::alpha = RxCompile::lower->or_(RxCompile::upper);
CharMatcher* RxCompile::alnum = RxCompile::digit->or_(RxCompile::alpha);
CharMatcher* RxCompile::punct =
CharMatcher::anyOf("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
CharMatcher* RxCompile::graph = RxCompile::punct->or_(RxCompile::alnum);
CharMatcher* RxCompile::print = RxCompile::graph->or_(CharMatcher::is(' '));
CharMatcher* RxCompile::xdigit = CharMatcher::anyOf("0123456789abcdefABCDEF");
CharMatcher* RxCompile::space = CharMatcher::anyOf(" \t\r\n\v\f");
CharMatcher* RxCompile::notSpace = RxCompile::space->negate();
CharMatcher* RxCompile::cntrl =
CharMatcher::inRange('\x00', '\x1f')->or_(CharMatcher::is('\x7f'));
CharMatcher* RxCompile::word = RxCompile::alnum->or_(CharMatcher::is('_'));
CharMatcher* RxCompile::notWord = RxCompile::word->negate();

static char tolower(char c)
	{
	return RxCompile::upper->matches(c) ? c + ('a' - 'A') : c;
	}

static char toupper(char c)
	{
	return RxCompile::lower->matches(c) ? c - ('a' - 'A') : c;
	}

static bool same(char c1, char c2, bool ignoringCase)
	{
	return ignoringCase ? tolower(c1) == tolower(c2) : c1 == c2;
	}

class PatEnd : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return -1;
		}
	};

class StartOfLine : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == 0 || s[si - 1] == '\n') ? si : -1;
		}
	int nextPossible(const char* s, int si, int sn) const override
		{
		if (++si == sn)
			return sn + 1;

		const char* next = strchr(s + si, '\n');
		return next == nullptr ? sn + 1 : next - s + 1;
		}
	};

class EndOfLine : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == sn || s[si] == '\r' || s[si] == '\n') ? si : -1;
		}
	};

class StartOfString : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == 0) ? si : -1;
		}
	int nextPossible(const char* s, int si, int sn) const override
		{
		return sn + 1;
		}
	};

class EndOfString : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == sn || (si == sn - 1 && s[si] == '\n')
			|| (si == sn - 2 && s[si] == '\r' && s[si + 1] == '\n'))
			? si : -1;
		}
	};

class StartOfWord : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == 0 || !RxCompile::word->matches(s[si - 1])) ? si : -1;
		}
	};

class EndOfWord : public Element
	{
public:
	int omatch(const char* s, int si, int sn) const override
		{
		return (si == sn || !RxCompile::word->matches(s[si])) ? si : -1;
		}
	};

class Backref : public Element
	{
public:
	Backref(int idx, bool ignoringCase) : idx(idx), ignoringCase(ignoringCase)
		{ }
	int omatch(const char* s, int si, int sn, Rxpart* res) const override
		{
		auto t = res[idx].s;
		int tn = res[idx].n;
		if (tn == -1 || si + tn > sn)
			return -1;
		for (int ti = 0; ti < tn; ++ti, ++si)
			if (!same(s[si], t[ti], ignoringCase))
				return -1;
		return si;
		}
private:
	int idx;
	bool ignoringCase;
	};

class Chars : public Element
	{
public:
	Chars(const char* chars, bool ignoringCase) : 
		chars(chars), ignoringCase(ignoringCase)
		{ }
	Chars(const char* chars, int sn, bool ignoringCase) :
		chars(chars, sn), ignoringCase(ignoringCase)
		{ }
	void add(const char* s, int sn)
		{
		chars += gcstring(s, sn);
		}
	int omatch(const char*s, int si, int sn) const override
		{
		if (si + chars.size() > sn)
			return -1;
		for (int ti = 0; ti < chars.size(); ++ti, ++si)
			if (!same(s[si], chars[ti], ignoringCase))
				return -1;
		return si;
		}
	int nextPossible(const char* s, int si, int sn) const override
		{
		int len = chars.size();
		for (++si; si <= sn - len; ++si)
			for (int i = 0; ; ++i)
				if (i == len)
					return si;
				else if (!same(s[si + i], chars[i], ignoringCase))
					break;
		return sn + 1;
		}
private:
	gcstring chars;
	bool ignoringCase;
	};

class CharClass : public Element
	{
public:
	CharClass(CharMatcher* cm, bool ignoringCase = false) :
		cm(cm), ignoringCase(ignoringCase)
		{ }
	int omatch(const char* s, int si, int sn) const override
		{
		if (si >= sn)
			return -1;
		return matches(s[si]) ? si + 1 : -1;
		}
	int nextPossible(const char* s, int si, int sn) const override
		{
		for (++si; si < sn; ++si)
			if (matches(s[si]))
				return si;
		return sn + 1;
		}
private:
	bool matches(char c) const
		{
		return ignoringCase
			? cm->matches(tolower(c)) || cm->matches(toupper(c))
			: cm->matches(c);
		}
	CharMatcher* cm;
	bool ignoringCase;
	};

class Branch : public Element
	{
public:
	Branch(int main, int alt) : main(main), alt(alt)
		{ }
	const int main;
	const int alt;
	};

class Jump : public Element
	{
public:
	Jump(int offset) : offset(offset)
		{ }
	const int offset;
	};

class Left : public Element
	{
public:
	Left(int idx) : idx(idx)
		{ }
	const int idx;
	};

class Right : public Element
	{
public:
	Right(int idx) : idx(idx)
		{ }
	const int idx;
	};

static Left LEFT0(0);
static Right RIGHT0(0);
static PatEnd PATEND;
static StartOfLine startOfLine;
static EndOfLine endOfLine;
static StartOfString startOfString;
static EndOfString endOfString;
static StartOfWord startOfWord;
static EndOfWord endOfWord;
static CharClass any(CharMatcher::noneOf("\r\n"));

char* rx_compile(const gcstring& s)
	{
	static CacheMap<10, gcstring, char*> cache;
	if (char** p = cache.get(s))
		return *p;
	return cache.put(s, RxCompile(s.ptr(), s.size()).compile());
	}

char* RxCompile::compile()
	{
	emit(&LEFT0);
	regexp();
	emit(&RIGHT0);
	emit(&PATEND);
	if (si < sn)
		except("regex: closing ) without opening (");
	return reinterpret_cast<char*>(&pat[0]);
	}

void RxCompile::regexp()
	{
	int start = pat.size();
	sequence();
	if (match('|'))
		{
		int len = pat.size() - start;
		insert(start, new Branch(1, len + 2));
		while (true)
			{
			start = pat.size();
			sequence();
			len = pat.size() - start;
			if (match("|"))
				{
				insert(start, new Branch(1, len + 2));
				insert(start, new Jump(len + 2));
				}
			else
				break;
			}
		insert(start, new Jump(len + 1));
		}
	}

void RxCompile::sequence()
	{
	while (si < sn && src[si] != '|' && src[si] != ')')
		element();
	}

void RxCompile::element()
	{
	if (match('^'))
		emit(&startOfLine);
	else if (match('$'))
		emit(&endOfLine);
	else if (match("\\A"))
		emit(&startOfString);
	else if (match("\\Z"))
		emit(&endOfString);
	else if (match("\\<"))
		emit(&startOfWord);
	else if (match("\\>"))
		emit(&endOfWord);
	else if (match("(?i)"))
		ignoringCase = true;
	else if (match("(?-i)"))
		ignoringCase = false;
	else if (match("(?q)"))
		quoted();
	else if (match("(?-q)"))
		;
	else
		{
		int start = pat.size();
		simple();
		int len = pat.size() - start;
		if (match("??"))
			insert(start, new Branch(len + 1, 1));
		else if (match('?'))
			insert(start, new Branch(1, len + 1));
		else if (match("+?"))
			emit(new Branch(1, -len));
		else if (match('+'))
			emit(new Branch(-len, 1));
		else if (match("*?"))
			{
			emit(new Branch(1, -len));
			insert(start, new Branch(len + 2, 1));
			}
		else if (match('*'))
			{
			emit(new Branch(-len, 1));
			insert(start, new Branch(1, len + 2));
			}
		}
	}

void RxCompile::quoted()
	{
	int start = si;
	char* pos = strstr(const_cast<char*>(src + si), "(?-q)");
	si = pos == nullptr ? sn : pos - src;
	emitChars(src + start, si - start);
	}

void RxCompile::simple()
	{
	if (match("."))
		emit(&any);
	else if (match("\\d"))
		emit(new CharClass(digit));
	else if (match("\\D"))
		emit(new CharClass(notDigit));
	else if (match("\\w"))
		emit(new CharClass(word));
	else if (match("\\W"))
		emit(new CharClass(notWord));
	else if (match("\\s"))
		emit(new CharClass(space));
	else if (match("\\S"))
		emit(new CharClass(notSpace));
	else if (matchBackref())
		{
		int i = src[si - 1] - '0';
		emit(new Backref(i, ignoringCase));
		}
	else if (match("["))
		{
		charClass();
		mustMatch(']');
		}
	else if (match("("))
		{
		int i = ++leftCount;
		emit(new Left(i));
		regexp();					// recurse
		emit(new Right(i));
		mustMatch(')');
		}
	else
		{
		if (si + 1 < sn)
			match("\\");
		emitChars(src + si, 1);
		++si;
		}
	}

void RxCompile::charClass()
	{
	bool negate = match('^');
	gcstring chars = "";
	if (match(']'))
		chars += "]";
	CharMatcher* cm = &CharMatcher::NONE;
	while (si < sn && src[si] != ']')
		{
		CharMatcher* elem;
		if (matchRange())
			{
			unsigned from = src[si - 3];
			unsigned to = src[si - 1];
			elem = (from < to)
				? CharMatcher::inRange(from, to)
				: &CharMatcher::NONE;
			}
		else if (match("\\d"))
			elem = digit;
		else if (match("\\D"))
			elem = notDigit;
		else if (match("\\w"))
			elem = word;
		else if (match("\\W"))
			elem = notWord;
		else if (match("\\s"))
			elem = space;
		else if (match("\\S"))
			elem = notSpace;
		else if (match("[:"))
			elem = posixClass();
		else
			{
			if (si + 1 < sn)
				match("\\");
			chars += gcstring(src + si, 1);
			si++;
			continue;
			}
		cm = cm->or_(elem);
		}
	if (!negate && cm == &CharMatcher::NONE && chars.size() == 1)
		{
		emitChars(chars.ptr(), 1);
		return;
		}
	if (chars.size() > 0)
		cm = cm->or_(CharMatcher::anyOf(chars));
	if (negate)
		cm = cm->negate();
	emit(new CharClass(cm, ignoringCase));
	}

CharMatcher* RxCompile::posixClass()
	{
	if (match("alpha:]"))
		return alpha;
	else if (match("alnum:]"))
		return alnum;
	else if (match("blank:]"))
		return blank;
	else if (match("cntrl:]"))
		return cntrl;
	else if (match("digit:]"))
		return digit;
	else if (match("graph:]"))
		return graph;
	else if (match("lower:]"))
		return lower;
	else if (match("print:]"))
		return print;
	else if (match("punct:]"))
		return punct;
	else if (match("space:]"))
		return space;
	else if (match("upper:]"))
		return upper;
	else if (match("xdigit:]"))
		return xdigit;
	else
		except("bad posix class");
	}

void RxCompile::emit(Element* e)
	{
	pat.push_back(e);
	inChars = false;
	}

void RxCompile::emitChars(const char* s, int n)
	{
	if (inChars && inCharsIgnoringCase == ignoringCase && !next1of("?*+"))
		static_cast<Chars*>(pat.back())->add(s, n);
	else
		{
		emit(new Chars(s, n, ignoringCase));
		inChars = true;
		inCharsIgnoringCase = ignoringCase;
		}
	}

void RxCompile::insert(int i, Element* e)
	{
	pat.insert(pat.begin() + i, e);
	inChars = false;
	}

bool RxCompile::match(char c)
	{
	if (src[si] != c)
		return false;
	++si;
	return true;
	}

bool RxCompile::match(const char* s)
	{
	if (strncmp(src + si, s, strlen(s)) != 0)
		return false;
	si += strlen(s);
	return true;
	}

void RxCompile::mustMatch(char c)
	{
	if (!match(c))
		except("regex: missing '" << c << "'");
	}

bool RxCompile::matchBackref()
	{
	if (si + 2 > sn || src[si] != '\\')
		return false;
	char c = src[si + 1];
	if (c < '1' || '9' < c)
		return false;
	si += 2;
	return true;
	}

bool RxCompile::matchRange()
	{
	if (si + 2 < sn && src[si + 1] == '-' && src[si + 2] != ']')
		{
		si += 3;
		return true;
		}
	else
		return false;
	}

bool RxCompile::next1of(const char* const set) const
	{
	return si + 1 < sn && strchr(set, src[si + 1]) != nullptr;
	}

/* next the stuff to match a pattern and a string
* amatch matches a specific point in a string against a compiled regular expression
* returns -1 if no match, else the position past the match
*/
class RxMatch
	{
public:
	RxMatch(const char* str, int len, Rxpart* pts)
		: s(str), n(len), part(pts ? pts : parts)
		{ }
	int amatch(int si, const Element** pat, IntList* alt_si, IntList* alt_pi) const;
	int amatch(int si, const Element** pat);
private:
	const char* s;				// string
	int n;						// length of string
	Rxpart* part;
	Rxpart parts[MAXPARTS];		// used if none passed in
	};

bool rx_match(const char* s, int n, int i, const char* pat, Rxpart* psubs)
	{
	IntList alt_si;
	IntList alt_pi;
	RxMatch match(s, n, psubs);
	const Element** p = (const Element**)pat;
	const Element* e = p[1];
	for (int si = i; si <= n; si = e->nextPossible(s, si, n))
		if (match.amatch(si, p, &alt_si, &alt_pi) != -1)
			return true;
	return false;
	}

bool rx_match_reverse(const char* s, int n, int i, const char* pat, Rxpart* psubs)
	{
	IntList alt_si;
	IntList alt_pi;
	RxMatch match(s, n, psubs);
	for (int si = i; si >= 0; si--)
		if (match.amatch(si, (const Element**)pat, &alt_si, &alt_pi) != -1)
			return true;
	return false;
	}

int rx_amatch(const char* s, int i, int n, const char* pat, Rxpart* psubs)
	{
	return RxMatch(s, n, psubs).amatch(i, (const Element**)pat);
	}

int RxMatch::amatch(int si, const Element** pat)
	{
	IntList alt_si;
	IntList alt_pi;
	return amatch(si, pat, &alt_si, &alt_pi);
	}

int RxMatch::amatch(int si, const Element** pat, 
	IntList* alt_si, IntList* alt_pi) const
	{
	int idx;
	for (int j = 0; j < MAXPARTS; ++j)
		part[j].n = -1;
	alt_si->clear();
	alt_pi->clear();
	for (int pi = 0; pat[pi] != &PATEND;)
		{
		const Element* e = pat[pi];
		if (typeid(*e) == typeid(Branch))
			{
			const Branch* b = dynamic_cast<const Branch*>(e);
			alt_pi->push_back(pi + b->alt);
			alt_si->push_back(si);
			pi += b->main;
			}
		else if (typeid(*e) == typeid(Jump))
			{
			pi += dynamic_cast<const Jump*>(e)->offset;
			}
		else if (typeid(*e) == typeid(Left))
			{
			idx = dynamic_cast<const Left*>(e)->idx;
			if (idx < MAXPARTS)
				part[idx].tmp = s + si;
			++pi;
			}
		else if (typeid(*e) == typeid(Right))
			{
			idx = dynamic_cast<const Right*>(e)->idx;
			if (idx < MAXPARTS)
				{
				part[idx].s = part[idx].tmp;
				part[idx].n = s + si - part[idx].tmp;
				}
			++pi;
			}
		else
			{
			si = e->omatch(s, si, n, part);
			if (si >= 0)
				++pi;
			else
				{
				if (alt_si->size() <= 0)
					return -1;
				si = alt_si->back();
				pi = alt_pi->back();
				alt_si->pop_back();
				alt_pi->pop_back();
				}
			}
		}
	return si;
	}

// replace ==========================================================

int rx_replen(const gcstring& rep, Rxpart* subs)
	{
	int nr = rep.size();
	if (rep[0] == '\\' && rep[1] == '=')
		return nr - 2;

	int len = 0;
	for (int i = 0; i < nr; ++i)
		{
		char c = rep[i];
		if (c == '&')
			{
			if (subs->n > 0)
				len += subs[0].n;
			}
		else if ('\\' == c && i + 1 < nr)
			{
			c = rep[++i];
			if (RxCompile::digit->matches(c))
				{
				if (subs[c - '0'].n > 0)
					len += subs[c - '0'].n;
				}
			else if (c != 'u' && c != 'l' && c != 'U' && c != 'L' && c != 'E')
				++len;
			}
		else
			++len;
		}
	return len;
	}

static char trcase(char& tr, char c)
	{
	c = tr == 'E' ? c :
		(tr == 'L' || tr == 'l' ? tolower(c) : toupper(c));
	if (tr == 'u' || tr == 'l')
		tr = 'E';
	return c;
	}

static char* insert(char* dst, Rxpart& part, char& tr)
	{
	int n = part.n;
	if (n <= 0)
		return dst;
	auto s = part.s;
	for (; n--; ++s)
		*dst++ = trcase(tr, *s);
	return dst;
	}

char* rx_mkrep(char* buf, const gcstring& rep, Rxpart* subs)
	{
	int nr = rep.size();
	if (rep[0] == '\\' && rep[1] == '=')
		{
		memcpy(buf, rep.ptr() + 2, nr - 2);
		return buf;
		}

	char tr = 'E';
	char *dst = buf;
	for (int i = 0; i < nr; ++i)
		{
		char c = rep[i];
		if (c == '&')
			dst = insert(dst, subs[0], tr);
		else if (c == '\\' && i + 1 < nr)
			{
			c = rep[++i];
			if (RxCompile::digit->matches(c))
				dst = insert(dst, subs[c - '0'], tr);
			else if (c == 'n')
				*dst++ = '\n';
			else if (c == 't')
				*dst++ = '\t';
			else if (c == '\\')
				*dst++ = '\\';
			else if (c == '&')
				*dst++ = '&';
			else if (c == 'u' || c == 'l' || c == 'U' || c == 'L' || c == 'E')
				tr = c;
			else
				*dst++ = c;
			}
		else
			*dst++ = trcase(tr, c);
		}
	*dst = 0;
	return buf;
	}

#include "testing.h"

struct Rxtest
	{
	const char* string;
	const char* pattern;
	bool result;
	};
static Rxtest rxtests[] =
	{
		{ "hello", "hello", true },
		{ "hello", "^hello$", true },

		{ "hello\nworld", "^hello$", true },
		{ "hello\nworld", "\\Ahello", true },
		{ "hello\nworld", "\\Aworld", false },
		{ "hello\nworld", "world\\Z", true },
		{ "hello\nworld", "hello\\Z", false },

		{ "hello\r\nworld", "^hello$", true },
		{ "hello\r\nworld", "\\Ahello", true },
		{ "hello\r\nworld", "\\Aworld", false },
		{ "hello\r\nworld", "world\\Z", true },
		{ "hello\r\nworld", "hello\\Z", false },

		{ "one_1 two_2\nthree_3", "\\<one_1\\>", true },
		{ "one_1 two_2\nthree_3", "\\<two_2\\>", true },
		{ "one_1 two_2\nthree_3", "\\<three_3\\>", true },
		{ "one_1 two_2\r\nthree_3", "\\<two_2\\>", true },
		{ "one_1 two_2\r\nthree_3", "\\<three_3\\>", true },
		{ "one_1 two_2\nthree_3", "\\<one\\>", false },
		{ "one_1 two_2\nthree_3", "\\<two\\>", false },

		{ "hello", "fred", false },
		{ "hello", "h.*o", true },
		{ "hello", "[a-z]ello", true },
		{ "hello", "[^0-9]ello", true },
		{ "hello", "ell", true },
		{ "hello", "^ell", false },
		{ "hello", "ell$", false },
		{ "heeeeeeeello", "^he+llo$", true },
		{ "heeeeeeeello", "^he*llo*", true },
		{ "hllo", "^he*llo$", true },
		{ "hllo", "^he?llo$", true },
		{ "hello", "^he?llo$", true },
		{ "heello", "^he?llo$", false },
		{ "+123.456", "^[+-][0-9]+[.][0123456789]*$", true },

		{ "0123456789", "^\\d+$", true },
		{ "0123456789", "\\D", false },
		{ "hello_123", "^\\w+$", true },
		{ "hello_123", "\\W", false },
		{ "hello \t\r\nworld", "^\\w+\\s+\\w+$", true },
		{ "!@#@!# \r\t{},()[];", "^\\W+$", true },
		{ "123adjf!@#", "^\\S+$", true },
		{ "123adjf!@#", "\\s", false },

		{ "()[]", "^\\(\\)\\[\\]$", true },
		{ "hello world", "^(hello|howdy) (\\w+)$", true },
		{ "ab", "(a|ab)b", true }, // alternation backtrack
		{ "abc", "x*c", true },
		{ "abc", "x*$", true },
		{ "abc", "x?$", true },
		{ "abc", "^x?", true },
		{ "abc", "^x*", true },
		{ "aBcD", "abcd", false },
		{ "aBcD", "(?i)abcd", true },
		{ "aBCd", "a(?i)bc(?-i)d", true },
		{ "aBCD", "a(?i)bc(?-i)D", true },
		{ "ABCD", "a(?i)bc(?-i)d", false },
		{ "abc", "a.c", true },
		{ "a.c", "(?q)a.c", true },
		{ "abc", "(?q)a.c", false },
		{ "a.cd", "(?q)a.c(?-q).", true },
		{ "abcd", "(?q)a.c(?-q).", false },
		{ "abc", "(?q)(", false },
		{ "ABC", "(?i)[A-Z]", true },
		{ "ABC", "(?i)[a-z]", true },
		{ "abc", "(?i)[A-Z]", true },
		{ "abc", "(?i)[a-z]", true },
		{ "\xaa", "[\xaa\xbb]", true },
		{ "\xaa", "(?i)[\xaa\xbb]", true },
		{ "\x8a", "(?i)\x9a", false },
		{ "\x8a", "(?i)[\x9a\xbb]", false }, // need \xbb to keep char class
		{ "\x8a", "[\x80-\x90]", true },
		{ "\x8a", "(?i)[\x80-\x90]", true }, // 0x8a is upper in latin charset
		{ "x", "(?i)[2-Y]", true },

		{ "aBc123", "^[[:alnum:]]+$", true },
		{ "aBc", "^[[:alpha:]]+$", true },
		{ "aBc  123", "^[[:alpha:]]+[[:blank:]]+[[:digit:]]+$", true },
		{ "\x01\x1f\x7f", "^[[:cntrl:]]+$", true },
		{ "a", "^[[:cntrl:]]+$", false },
		{ "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~azAZ19", "^[[:graph:]]+$", true },
		{ "\x01", "^[[:graph:]]+$", false },
		{ "abc", "^[[:lower:]]+$", true },
		{ "ABC", "^[[:lower:]]+$", false },
		{ "ABC", "^[[:upper:]]+$", true },
		{ "19abcdEF \t!", "^[[:xdigit:]]+[[:space:]]+[[:punct:]]?$", true }
	};

class test_regexp : public Tests
	{
	TEST(0, match)
		{
		for (int i = 0; i < sizeof rxtests / sizeof(Rxtest); ++i)
			{
			const char* pattern = rxtests[i].pattern;
			const char* pat = rx_compile(pattern);
			const char* string = rxtests[i].string;
			except_if(rxtests[i].result != rx_match(string, strlen(string), 0, pat, nullptr),
				string << " =~ " << pattern << " should be " << (rxtests[i].result ? "true" : "false"));
			}
		}
	TEST(1, replace)
		{
		char* pat = rx_compile("(\\w+) (\\w+)");
		Rxpart parts[MAXPARTS];
		auto s = "hello WORLD";
		rx_amatch(s, 0, strlen(s), pat, parts);
		const char* rep = "\\u\\1 \\l\\2 \\U\\1 \\L\\2 \\E\\1 \\2";
		size_t replen = rx_replen(rep, parts);
		char buf[200];
		rx_mkrep(buf, rep, parts);
		asserteq(strlen(buf), replen);
		verify(0 == strcmp(buf, "Hello wORLD HELLO world hello WORLD"));
		}
	TEST(2, group_repetition_bug)
		{
		char* pat = rx_compile("(\\w+ )+");
		Rxpart parts[MAXPARTS];
		auto s = "hello world ";
		rx_match(s, strlen(s), 0, pat, parts);
		asserteq(parts[0].s, s);
		asserteq(parts[0].n, 12);
		asserteq(parts[1].s, s + 6);
		asserteq(parts[1].n, 6);
		}
	TEST(3, escape_nul_bug)
		{
		asserteq(rx_replen("\\", nullptr), 1);
		}
	};
REGISTER(test_regexp);

class test_regexp2 : public Tests
	{
	TEST(0, main)
		{
		char* pat = rx_compile(gcstring("[\x00-\xff]+", 6));
		Rxpart parts[MAXPARTS];
		auto s = "axb";
		rx_amatch(s, 0, 3, pat, parts);
		asserteq(parts[0].s, s);
		asserteq(parts[0].n, 3);
		}
	};
REGISTER(test_regexp2);

class test_element : public Tests
	{
	TEST(0, posix)
		{
		asserteq(RxCompile::blank->matches(' '), true);
		asserteq(RxCompile::blank->matches('\t'), true);
		asserteq(RxCompile::blank->matches('\0'), false);
		asserteq(RxCompile::blank->matches('a'), false);

		asserteq(RxCompile::digit->matches('0'), true);
		asserteq(RxCompile::digit->matches('9'), true);
		asserteq(RxCompile::digit->matches('\0'), false);
		asserteq(RxCompile::digit->matches('a'), false);

		asserteq(RxCompile::notDigit->matches('0'), false);
		asserteq(RxCompile::notDigit->matches('9'), false);
		asserteq(RxCompile::notDigit->matches('\0'), true);
		asserteq(RxCompile::notDigit->matches('a'), true);

		asserteq(RxCompile::lower->matches('a'), true);
		asserteq(RxCompile::lower->matches('z'), true);
		asserteq(RxCompile::lower->matches('A'), false);
		asserteq(RxCompile::lower->matches('\0'), false);

		asserteq(RxCompile::upper->matches('A'), true);
		asserteq(RxCompile::upper->matches('Z'), true);
		asserteq(RxCompile::upper->matches('a'), false);
		asserteq(RxCompile::upper->matches('\0'), false);

		asserteq(RxCompile::alpha->matches('a'), true);
		asserteq(RxCompile::alpha->matches('A'), true);
		asserteq(RxCompile::alpha->matches('1'), false);
		asserteq(RxCompile::alpha->matches('\n'), false);

		asserteq(RxCompile::alnum->matches('a'), true);
		asserteq(RxCompile::alnum->matches('A'), true);
		asserteq(RxCompile::alnum->matches('1'), true);
		asserteq(RxCompile::alnum->matches('\n'), false);

		asserteq(RxCompile::punct->matches('!'), true);
		asserteq(RxCompile::punct->matches('\\'), true);
		asserteq(RxCompile::punct->matches('a'), false);
		asserteq(RxCompile::punct->matches('\n'), false);

		asserteq(RxCompile::graph->matches('!'), true);
		asserteq(RxCompile::graph->matches('1'), true);
		asserteq(RxCompile::graph->matches('a'), true);
		asserteq(RxCompile::graph->matches(' '), false);
		asserteq(RxCompile::graph->matches('\n'), false);

		asserteq(RxCompile::print->matches('!'), true);
		asserteq(RxCompile::print->matches('1'), true);
		asserteq(RxCompile::print->matches('a'), true);
		asserteq(RxCompile::print->matches(' '), true);
		asserteq(RxCompile::print->matches('\n'), false);

		asserteq(RxCompile::xdigit->matches('0'), true);
		asserteq(RxCompile::xdigit->matches('a'), true);
		asserteq(RxCompile::xdigit->matches('g'), false);
		asserteq(RxCompile::xdigit->matches('\n'), false);

		asserteq(RxCompile::space->matches(' '), true);
		asserteq(RxCompile::space->matches('\t'), true);
		asserteq(RxCompile::space->matches('\n'), true);
		asserteq(RxCompile::space->matches('a'), false);

		asserteq(RxCompile::notSpace->matches(' '), false);
		asserteq(RxCompile::notSpace->matches('\t'), false);
		asserteq(RxCompile::notSpace->matches('\n'), false);
		asserteq(RxCompile::notSpace->matches('a'), true);

		asserteq(RxCompile::cntrl->matches(' '), false);
		asserteq(RxCompile::cntrl->matches('a'), false);
		asserteq(RxCompile::cntrl->matches('\n'), true);
		asserteq(RxCompile::cntrl->matches('\x00'), true);

		asserteq(RxCompile::word->matches('a'), true);
		asserteq(RxCompile::word->matches('1'), true);
		asserteq(RxCompile::word->matches('_'), true);
		asserteq(RxCompile::word->matches('\n'), false);
		asserteq(RxCompile::word->matches('('), false);

		asserteq(RxCompile::notWord->matches('a'), false);
		asserteq(RxCompile::notWord->matches('1'), false);
		asserteq(RxCompile::notWord->matches('_'), false);
		asserteq(RxCompile::notWord->matches('\n'), true);
		asserteq(RxCompile::notWord->matches('('), true);
		}
	TEST(1, omatch)
		{
		asserteq(startOfLine.omatch("abc\nabc", 0, 7), 0);
		asserteq(startOfLine.omatch("abc\nabc", 1, 7), -1);
		asserteq(startOfLine.omatch("abc\nabc", 4, 7), 4);

		asserteq(endOfLine.omatch("abc\nabc\r\nabc", 0, 12), -1);
		asserteq(endOfLine.omatch("abc\nabc\r\nabc", 3, 12), 3);
		asserteq(endOfLine.omatch("abc\nabc\r\nabc", 7, 12), 7);
		asserteq(endOfLine.omatch("abc\nabc\r\nabc", 12, 12), 12);

		asserteq(startOfString.omatch("abc\nabc", 0, 7), 0);
		asserteq(startOfString.omatch("abc\nabc", 5, 7), -1);

		asserteq(endOfString.omatch("abc\r\n", 5, 5), 5);
		asserteq(endOfString.omatch("abc\r\n", 4, 5), 4);
		asserteq(endOfString.omatch("abc\r\n", 3, 5), 3);
		asserteq(endOfString.omatch("abc\r\n", 2, 5), -1);

		asserteq(startOfWord.omatch("abc abc\rabc", 0, 11), 0);
		asserteq(startOfWord.omatch("abc abc\rabc", 4, 11), 4);
		asserteq(startOfWord.omatch("abc abc\rabc", 8, 11), 8);
		asserteq(startOfWord.omatch("abc abc\rabc", 1, 11), -1);

		asserteq(endOfWord.omatch("abc abc\rabc", 3, 11), 3);
		asserteq(endOfWord.omatch("abc abc\rabc", 7, 11), 7);
		asserteq(endOfWord.omatch("abc abc\rabc", 11, 11), 11);
		asserteq(endOfWord.omatch("abc abc\rabc", 1, 11), -1);

		asserteq(any.omatch("abc abc\r\n", 0, 9), 1);
		asserteq(any.omatch("abc abc\r\n", 3, 9), 4);
		asserteq(any.omatch("abc abc\r\n", 7, 9), -1);
		asserteq(any.omatch("abc abc\r\n", 8, 9), -1);

		Chars eChars1("aBc", false);
		asserteq(eChars1.omatch("abc aBc", 0, 7), -1);
		asserteq(eChars1.omatch("abc aBc", 4, 7), 7);
		asserteq(eChars1.omatch("abc aBc", 5, 7), -1);
		Chars eChars2("aBc", true);
		asserteq(eChars2.omatch("abc aBc", 0, 7), 3);
		asserteq(eChars2.omatch("abc aBc", 4, 7), 7);
		asserteq(eChars2.omatch("abc aBc", 5, 7), -1);
		asserteq(eChars2.omatch("abc aBc", 7, 7), -1);

		CharMatcher* cm = CharMatcher::anyOf("acd")->or_(CharMatcher::inRange('1', '9'));
		CharClass eCharClass1(cm, false);
		asserteq(eCharClass1.omatch("Aa1\n", 0, 4), -1);
		asserteq(eCharClass1.omatch("Aa1\n", 1, 4), 2);
		asserteq(eCharClass1.omatch("Aa1\n", 2, 4), 3);
		asserteq(eCharClass1.omatch("Aa1\n", 3, 4), -1);
		asserteq(eCharClass1.omatch("Aa1\n", 4, 4), -1);
		CharClass eCharClass2(cm, true);
		asserteq(eCharClass2.omatch("Aa1\n", 0, 4), 1);
		asserteq(eCharClass2.omatch("Aa1\n", 1, 4), 2);
		asserteq(eCharClass2.omatch("Aa1\n", 2, 4), 3);
		asserteq(eCharClass2.omatch("Aa1\n", 3, 4), -1);
		asserteq(eCharClass2.omatch("Aa1\n", 4, 4), -1);

		Rxpart parts[2];
		const char* s = "hello world !!";
		parts[0].s = s;
		parts[0].n = 5;
		parts[1].s = s + 6;
		parts[1].n = 5;
		Backref eBackref1(1, false);
		asserteq(eBackref1.omatch("  World\nworld", 2, 13, parts), -1);
		asserteq(eBackref1.omatch("  World\nworld", 8, 13, parts), 13);
		asserteq(eBackref1.omatch("  World\nworld", 9, 13, parts), -1);
		asserteq(eBackref1.omatch("  World\nworld", 13, 13, parts), -1);
		Backref eBackref2(1, true);
		asserteq(eBackref2.omatch("  World\nworld", 2, 13, parts), 7);
		asserteq(eBackref2.omatch("  World\nworld", 8, 13, parts), 13);
		asserteq(eBackref2.omatch("  World\nworld", 9, 13, parts), -1);
		asserteq(eBackref2.omatch("  World\nworld", 13, 13, parts), -1);
		}
	TEST(2, nextPossible)
		{
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 0, 9), 5);
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 1, 9), 5);
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 4, 9), 6);
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 5, 9), 10);
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 6, 9), 10);
		asserteq(startOfLine.nextPossible("\nabc\n\nabc", 9, 9), 10);

		asserteq(startOfString.nextPossible("abc", 0, 3), 4);
		asserteq(startOfString.nextPossible("abc", 3, 3), 4);

		Chars eChars1("aBc", false);
		asserteq(eChars1.nextPossible("abc aBc", 0, 7), 4);
		asserteq(eChars1.nextPossible("abc aBc", 4, 7), 8);
		asserteq(eChars1.nextPossible("abc aBc", 5, 7), 8);
		Chars eChars2("aBc", true);
		asserteq(eChars2.nextPossible("abc aBc", 0, 7), 4);
		asserteq(eChars2.nextPossible("abc aBc", 4, 7), 8);
		asserteq(eChars2.nextPossible("abc aBc", 5, 7), 8);
		asserteq(eChars2.nextPossible("abc aBc", 7, 7), 8);

		CharMatcher* cm = CharMatcher::anyOf("acd")->or_(CharMatcher::inRange('1', '9'));
		CharClass eCharClass1(cm, false);
		asserteq(eCharClass1.nextPossible("AC1\n", 0, 4), 2);
		asserteq(eCharClass1.nextPossible("AC1\n", 1, 4), 2);
		asserteq(eCharClass1.nextPossible("AC1\n", 2, 4), 5);
		asserteq(eCharClass1.nextPossible("AC1\n", 3, 4), 5);
		asserteq(eCharClass1.nextPossible("AC1\n", 4, 4), 5);
		CharClass eCharClass2(cm, true);
		asserteq(eCharClass2.nextPossible("AC1\n", 0, 4), 1);
		asserteq(eCharClass2.nextPossible("AC1\n", 1, 4), 2);
		asserteq(eCharClass2.nextPossible("AC1\n", 2, 4), 5);
		asserteq(eCharClass2.nextPossible("AC1\n", 3, 4), 5);
		asserteq(eCharClass2.nextPossible("AC1\n", 4, 4), 5);
		}
	};
REGISTER(test_element);