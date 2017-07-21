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

#include "scanner.h"
#include "sunumber.h"
#include <ctype.h>
#include <algorithm>
#include "except.h"
#include "opcodes.h"

enum { OK = 1, DIG, ID, GO, WHITE, END };

char cclass_[256] =
	{
	END, 0, 0, 0, 0, 0, 0, 0,						// NUL ...
	0, WHITE, WHITE, WHITE, WHITE, WHITE, 0, 0,		// BS, HT, LF ...
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	WHITE, GO, GO, GO, GO, GO, GO, GO,				// SP ! " # $ % & '
	OK, OK, GO, GO, OK, GO, GO, GO,					// ( ) * + , - . /
	DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG,			// 0 1 2 3 4 5 6 7
	DIG, DIG, GO, OK, GO, GO, GO, OK,				// 8 9 : ; < = > ?
	OK, ID, ID, ID, ID, ID, ID, ID,					// @ A B C D E F G
	ID, ID, ID, ID, ID, ID, ID, ID,					// H I J K L M N O
	ID, ID, ID, ID, ID, ID, ID, ID,					// P Q R S T U V W
	ID, ID, ID, OK, 0, OK, GO, ID,					// X Y Z [ \ ] ^ _
	GO, ID, ID, ID, ID, ID, ID, ID,					// ` a b c d e f g
	ID, ID, ID, ID, ID, ID, ID, ID,					// h i j k l m n o
	ID, ID, ID, ID, ID, ID, ID, ID,					// p q r s t u v w
	ID, ID, ID, OK, GO, OK, GO, 0					// x y z { | } ~ DEL
	};
inline char cclass(char c)
	{
	return cclass_[static_cast<unsigned char>(c)];
	}

bool iswhite(const char* s)
	{
	while (cclass(*s) == WHITE)
		++s;
	return *s;
	}

void scanner_locale_changed()
	{
	// called by su_setlocale
	for (int c = 0; c < 256; ++c)
		if (isalpha(c) || c == '_')
			cclass_[c] = ID;
		else if (cclass_[c] == ID)
			cclass_[c] = 0;
	}

Scanner::Scanner(const char* s, int i, CodeVisitor* v)
	: source(s), si(i), visitor(v), buf(1024)
	{
	}

Scanner::Scanner(const Scanner* sc) : source(sc->source), si(sc->si)
	{}

static bool isquote(char c)
	{
	return c == '"' || c == '\'' || c == '`';
	}

int Scanner::ahead() const
	{
	Scanner tmp(this);
	int token;
	do
		{
		if (isquote(tmp.source[tmp.si])) // avoid actually processing strings
			return T_STRING;
		token = tmp.nextall();
		}
		while (token == T_WHITE || token == T_COMMENT || token == T_NEWLINE);
	return tmp.keyword ? tmp.keyword : token;
	}

int Scanner::aheadnl() const
	{
	Scanner tmp(this);
	int token;
	do
		{
		if (isquote(tmp.source[tmp.si])) // avoid actually processing strings
			return T_STRING;
		token = tmp.nextall();
		}
		while (token == T_WHITE || token == T_COMMENT);
	return tmp.keyword ? tmp.keyword : token;
	}

int Scanner::next()
	{
	int token;
	do
		token = nextall();
		while (token == T_WHITE || token == T_COMMENT);
	return token;
	}

int Scanner::nextall()
	{
	int state = 0;

	prev = si; keyword = 0; value = "";
	for (;;)
		{
		switch (state)
			{
		case 0 :
			switch (cclass(source[si]))
				{
			case OK :
				return source[si++];
			case DIG :
				state = '9';
				break ;
			case ID :
				state = '_';
				break ;
			case GO :
				state = source[si++];
				break ;
			case WHITE :
				state = ' ';
				break ;
			case END :
				prev = si + 1;
				return Eof;
			default :
				prev = si++;
				err = "invalid character";
				return T_ERROR;
				}
			break ;
		case ' ' :
			{
			bool eol = false;
			for (; source[si] && cclass(source[si]) == WHITE; ++si)
				if (source[si] == '\r' || source[si] == '\n')
					eol = true;
			return eol ? T_NEWLINE : T_WHITE;
			}
		case '=' :
			switch (source[si])
				{
			case '=' : ++si; return I_IS;
			case '~' : ++si; return I_MATCH;
			default : return I_EQ;
				}
		case '~' :
			return I_BITNOT;
		case '!' :
			switch (source[si])
				{
			case '=' : ++si; return I_ISNT;
			case '~' : ++si; return I_MATCHNOT;
			default : return I_NOT;
				}
		case '<' :
			switch (source[si])
				{
			case '<' :
				if (source[++si] == '=')
					{ ++si; return I_LSHIFTEQ; }
				else
					return I_LSHIFT;
			case '>' : ++si; return I_ISNT;
			case '=' : ++si; return I_LTE;
			default : return I_LT;
				}
		case '>' :
			switch (source[si])
				{
			case '>' :
				if (source[++si] == '=')
					{ ++si; return I_RSHIFTEQ; }
				else
					return I_RSHIFT;
			case '=' : ++si; return I_GTE;
			default : return I_GT;
				}
		case '|' :
			switch (source[si])
				{
			case '|' : ++si; return T_OR;
			case '=' : ++si; return I_BITOREQ;
			default : return I_BITOR;
				}
		case '&' :
			switch (source[si])
				{
			case '&' : ++si; return T_AND;
			case '=' : ++si; return I_BITANDEQ;
			default : return I_BITAND;
				}
		case '^' :
			switch (source[si])
				{
			case '=' : ++si; return I_BITXOREQ;
			default : return I_BITXOR;
				}
		case '-' :
			switch (source[si])
				{
			case '-' : ++si; return I_PREDEC;
			case '=' : ++si; return I_SUBEQ;
			default : return I_SUB;
				}
		case '+' :
			switch (source[si])
				{
			case '+' : ++si; return I_PREINC;
			case '=' : ++si; return I_ADDEQ;
			default : return I_ADD;
				}
		case '/' :
			switch (source[si])
				{
			case '/' :
				// rest of line is comment
				for (; source[si] && '\n' != source[si]; ++si)
					;
				return T_COMMENT;
			case '*' :
				for (++si; source[si] && (source[si] != '*' ||
					source[si + 1] != '/'); ++si)
					;
				if (source[si])
					si += 2;
				return T_COMMENT;
			case '=' : ++si; return I_DIVEQ;
			default : return I_DIV;
				}
		case '*' :
			if ('=' == source[si])
				{ ++si; return I_MULEQ; }
			else
				return I_MUL;
		case '%' :
			if ('=' == source[si])
				{ ++si; return I_MODEQ; }
			else
				return I_MOD;
		case '$' :
			if ('=' == source[si])
				{ ++si; return I_CATEQ; }
			else
				return I_CAT;
		case ':' :
			if (':' == source[si])
				{ ++si; return T_RANGELEN; }
			else
				return ':';
		case '_' :
			while (ID == cclass(source[si]) || DIG == cclass(source[si]))
				++si;
			if (source[si] == '?' || source[si] == '!')
				++si;
			buf.clear().add(source + prev, si - prev);
			value = buf.str();
			keyword = keywords(value);
			if (source[si] != ':' ||
				! (keyword == I_IS || keyword == I_ISNT ||
					keyword == T_AND || keyword == T_OR || keyword == I_NOT))
				{
				if (keyword && keyword < KEYWORDS)
					return keyword;
				}
			return T_IDENTIFIER;
		case '9' :
			buf.clear();
			if (source[si] == '0' && tolower(source[si + 1]) == 'x')
				{
				si += 2;
				while (isxdigit(source[si]))
					++si;
				buf.add(source + prev, si - prev);
				len = buf.size();
				}
			else
				{
				len = numlen(source + si);
				verify(len > 0);
				if (source[si + len - 1] == '.' && iswhite(source + si + len))
					--len;
				buf.add(source + si, len);
				si += len;
				}
			value = buf.str();
			return T_NUMBER;
		case '.' :
			if (DIG == cclass(source[si]))
				--si, state = '9';
			else if ('.' == source[si])
				{ ++si; return T_RANGETO; }
			else
				return '.';
			break ;
		case '`' :
			while (source[si] && source[si] != '`')
				++si;
			buf.clear().add(source + prev + 1, si - prev - 1);
			if (source[si])
				++si;	// skip closing quote
			value = buf.str();
			len = buf.size();
			return T_STRING;
		case '"' :
		case '\'' :
			{
			buf.clear();
			int from = si;
			for (; source[si] && source[si] != state; ++si)
				if (source[si] == '\\')
					{
					buf.add(source + from, si - from);
					buf.add(doesc(source, si));
					from = si + 1;
					}
			buf.add(source + from, si - from);
			if (source[si])
				++si;	// skip closing quote
			value = buf.str();
			len = buf.size();
			return T_STRING;
			}
		case '#':
			if (ID != cclass(source[si]))
				return '#';
			while (ID == cclass(source[si]) || DIG == cclass(source[si]))
				++si;
			if (source[si] == '?' || source[si] == '!')
				++si;
			buf.clear().add(source + prev + 1, si - prev - 1);
			value = buf.str();
			len = buf.size();
			return T_STRING;
		default :
			unreachable();
			}
		}
	}

inline int hexval(char c)
	{ return c <= '9' ? c - '0' : tolower(c) - 'a' + 10; }

inline int octval(char c)
	{ return c - '0'; }

inline bool isodigit(char c)
	{ return '0' <= c && c <= '7'; }

// should be called with i pointing at backslash
/* static */ char Scanner::doesc(const char* src, int& i)
	{
	++i; // backslash
	switch (src[i])
		{
	case 'n' :
		return '\n';
	case 't' :
		return '\t';
	case 'r' :
		return '\r';
	case 'x' :
		if (isxdigit(src[i + 1]) && isxdigit(src[i + 2]))
			{
			i += 2;
			return 16 * hexval(src[i - 1]) + hexval(src[i]);
			}
		else
			return src[--i];
	case '\\' :
	case '"' :
	case '\'' :
		return src[i];
	default :
		if (isodigit(src[i]) && isodigit(src[i + 1]) && isodigit(src[i + 2]))
			{
			i += 2;
			return octval(src[i]) + 	8 * octval(src[i-1]) + 64 * octval(src[i-2]);
			}
		else
			return src[--i];
		}
	}

struct Keyword
	{
	const char* word;
	int id;
	};
static Keyword words[] =
	{
	{ "False", K_FALSE }, { "True", K_TRUE },
	{ "and", T_AND }, { "bool", K_BOOL}, { "break", K_BREAK },
	{ "buffer", K_BUFFER }, { "callback", K_CALLBACK },
	{ "case", K_CASE }, { "catch", K_CATCH }, { "char", K_INT8 },
	{ "class", K_CLASS }, { "continue", K_CONTINUE },
	{ "default", K_DEFAULT }, { "dll", K_DLL }, { "do", K_DO },
	{ "double", K_DOUBLE }, { "else", K_ELSE }, { "false", K_FALSE },
	{ "float", K_FLOAT }, { "for", K_FOR },
	{ "forever", K_FOREVER }, { "function", K_FUNCTION }, { "gdiobj", K_GDIOBJ },
	{ "handle", K_HANDLE }, { "if", K_IF }, 	{ "in", K_IN },
	{ "int16", K_INT16 }, { "int32", K_INT32 }, { "int64", K_INT64 }, { "int8", K_INT8 },
	{ "is", I_IS }, { "isnt", I_ISNT }, { "long", K_INT32 },
	{ "new", K_NEW }, { "not", I_NOT }, { "or", T_OR }, { "pointer", K_POINTER },
	{ "resource", K_RESOURCE },	{ "return", K_RETURN },
	{ "short", K_INT16 }, { "string", K_STRING }, { "struct", K_STRUCT },
	{ "super", K_SUPER }, { "switch", K_SWITCH }, { "this", K_THIS },
	{ "throw", K_THROW }, { "true", K_TRUE }, { "try", K_TRY },
	{ "void", K_VOID }, { "while", K_WHILE },
	{ "xor", I_ISNT }
	};
const int nwords = sizeof words / sizeof (Keyword);

inline bool lt(const char* ss, const char* tt)
	{
	const unsigned char* s = reinterpret_cast<const unsigned char*>(ss);
	const unsigned char* t = reinterpret_cast<const unsigned char*>(tt);
	for (; *s && *s == *t; ++s, ++t)
		;
	return *s < *t;
	}
struct ScannerLt
	{
	bool operator()(const Keyword& k1, const Keyword& k2)
		{ return lt(k1.word, k2.word); }
	bool operator()(const Keyword& k, const char* s)
		{ return lt(k.word, s); }
	bool operator()(const char* s, const Keyword& k)
		{ return lt(s, k.word); }
	};

int Scanner::keywords(const char* s)
	{
	std::pair<Keyword*,Keyword*> p = std::equal_range(words, words + nwords, s, ScannerLt());
	return p.first < p.second ? p.first->id : 0;
	}

#include "testing.h"
#include "gcstring.h"

static const char* input = "and break case catch continue class callback default dll do \
	else for forever function if is isnt or not \
	new switch struct super return throw try while \
	true True false False \
	== = =~ ~ != !~ ! <<= << <> <= < \
	>>= >> >= > || |= | && &= &  \
	^= ^ -- -= - ++ += + /= / \
	*= * %= % $= $ name _name name123 '\\Ahello\\'world\\Z' \
	\"string\" 123 123name .name  Name Name123 name? 1$2 +1 num=1 \
	num+=1 1%2 #id /*comments*/ //comments";

struct Result
	{
	int token;
	const char* value;
	};

static Result results[] =
	{
	{ T_AND, 0 }, { K_BREAK, 0 }, { K_CASE, 0 }, { K_CATCH, 0 },
	{ K_CONTINUE, 0 }, { K_CLASS, 0 }, { K_CALLBACK, 0 },
	{ K_DEFAULT, 0 }, { K_DLL, 0 }, { K_DO, 0 }, { K_ELSE, 0 },
	{ K_FOR, 0 }, { K_FOREVER, 0 }, { K_FUNCTION, 0 },
	{ K_IF, 0 }, { I_IS, 0 }, { I_ISNT, 0 }, { T_OR, 0 }, { I_NOT, 0 },
	{ K_NEW, 0 }, { K_SWITCH, 0 }, { K_STRUCT, 0 }, { K_SUPER, 0 },
	{ K_RETURN, 0 }, { K_THROW, 0 }, { K_TRY, 0 }, { K_WHILE, 0 },
	{ K_TRUE, 0 }, { K_TRUE, 0 }, { K_FALSE, 0 }, { K_FALSE, 0 },
	{ I_IS, 0 }, { I_EQ, 0 }, { I_MATCH, 0 }, { I_BITNOT, 0 },
	{ I_ISNT, 0 }, { I_MATCHNOT, 0 }, { I_NOT, 0 },
	{ I_LSHIFTEQ, 0 }, { I_LSHIFT, 0 },	{ I_ISNT, 0 }, { I_LTE, 0 },
	{ I_LT, 0 }, { I_RSHIFTEQ, 0 }, { I_RSHIFT, 0 }, { I_GTE, 0 },
	{ I_GT, 0 }, { T_OR, 0 }, { I_BITOREQ, 0 }, { I_BITOR, 0 },
	{ T_AND, 0 }, { I_BITANDEQ, 0 }, { I_BITAND, 0 }, { I_BITXOREQ, 0 }, { I_BITXOR, 0 }, { I_PREDEC, 0 }, { I_SUBEQ, 0 }, { I_SUB, 0 },
	{ I_PREINC, 0 }, { I_ADDEQ, 0 }, { I_ADD, 0 }, { I_DIVEQ, 0 },
	{ I_DIV, 0 }, { I_MULEQ, 0 }, { I_MUL, 0 }, { I_MODEQ, 0 },
	{ I_MOD, 0 }, { I_CATEQ, 0 }, { I_CAT, 0 }, { T_IDENTIFIER, "name" },
	{ T_IDENTIFIER, "_name"}, {T_IDENTIFIER, "name123"},
	{ T_STRING, "\\Ahello'world\\Z" }, { T_STRING, "string"},
	{ T_NUMBER, "123" }, { T_NUMBER, "123" },
	{ T_IDENTIFIER, "name" }, { '.', 0 },
	{ T_IDENTIFIER, "name" }, { T_IDENTIFIER, "Name" },
	{ T_IDENTIFIER, "Name123" }, { T_IDENTIFIER, "name?" },
	{ T_NUMBER, "1" }, { I_CAT, 0 }, { T_NUMBER, "2" }, { I_ADD, 0 },
	{ T_NUMBER, "1" }, { T_IDENTIFIER, "num" }, { I_EQ, 0 }, { T_NUMBER, "1" },
	{ T_IDENTIFIER, "num" }, { I_ADDEQ, 0 }, { T_NUMBER, "1" },
	{ T_NUMBER, "1" }, { I_MOD, 0 }, {T_NUMBER, "2" }, { T_STRING, "id" }
	};

class test_scanner : public Tests
	{
	TEST(1, scan)
		{
		int i, token;
		Scanner sc(input);
		for (i = 0; Eof != (token = sc.next()); ++i)
			{
			asserteq(results[i].token,
				(results[i].token < KEYWORDS ? token : sc.keyword));
			if (results[i].value)
				asserteq(gcstring(sc.value), gcstring(results[i].value));
			}
		verify(i == sizeof results / sizeof(Result));
		}
	};
REGISTER(test_scanner);
