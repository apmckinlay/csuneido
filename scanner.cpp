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
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "except.h"

enum { OK = 1, DIG, ID, GO, WHITE, END };

char cclass_[256] =
	{
	END, 0, 0, 0, 0, 0, 0, 0,						// NUL ...
	0, WHITE, WHITE, WHITE, WHITE, WHITE, 0, 0,		// BS, HT, LF ...
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	WHITE, GO, GO, OK, GO, GO, GO, GO,				// SP ! " # $ % & '
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

void scanner_locale_changed()
	{
	// called by su_setlocale
	for (int c = 0; c < 256; ++c)
		if (isalpha(c) || c == '_')
			cclass_[c] = ID;
		else if (cclass_[c] == ID)
			cclass_[c] = 0;
	}

Scanner::Scanner(char* s, int i, CodeVisitor* v)
	: prev(0), value(""), err(""), source(s), si(i), keyword(0), visitor(v)
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
	char* dst;
	char* lim;
	int state;
	char quote;

	prev = si; state = 0; keyword = 0; value = "";
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
			break ;
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
			for (dst = buf; ID == cclass(source[si]) ||
				DIG == cclass(source[si]); ++si)
				{
				*dst++ = source[si];
				}
			if (source[si] == '?' || source[si] == '!')
				*dst++ = source[si++];
			*dst = 0;
			value = buf;
			keyword = keywords(buf);
			if (source[si] != ':' ||
				! (keyword == I_IS || keyword == I_ISNT ||
					keyword == T_AND || keyword == T_OR || keyword == I_NOT))
				{
				if (keyword && keyword < KEYWORDS)
					return keyword;
				}
			return T_IDENTIFIER;
		case '9' :
			if (source[si] == '0' && tolower(source[si + 1]) == 'x')
				{
				dst = buf;
				*dst++ = source[si++];
				*dst++ = source[si++];
				while (isxdigit(source[si]))
					*dst++ = source[si++];
				*dst = 0;
				len = dst - buf;
				}
			else
				{
				len = numlen(source + si);
				verify(len > 0);
				memcpy(buf, source + si, len);
				if (buf[len - 1] == '.')
					--len;
				buf[len] = 0;
				si += len;
				}
			value = buf;
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
			for (dst = buf, lim = dst + buflen; 
				source[si] && source[si] != '`'; ++si)
				{
				if (dst >= lim)
					{
					prev = si;
					err = "string literal too long";
					return T_ERROR;
					}
				*dst++ = source[si];
				}
			*dst = 0;
			if (source[si])
				++si;	// skip closing quote
			value = buf;
			len = dst - buf;
			return T_STRING;
		case '"' :
		case '\'' :
			quote = state;
			for (dst = buf, lim = dst + buflen; 
				source[si] && source[si] != quote; ++si)
				{
				if (dst >= lim)
					{
					prev = si;
					err = "string literal too long";
					return T_ERROR;
					}
				else if ('\\' == source[si])
					*dst++ = doesc();
				else
					*dst++ = source[si];
				}
			*dst = 0;
			if (source[si])
				++si;	// skip closing quote
			value = buf;
			len = dst - buf;
			return T_STRING;
		default :
			unreachable();
			}
		}
	}

inline int hexval(char c)
	{ return c <= '9' ? c - '0' : tolower(c) - 'a' + 10; }

inline bool isodigit(char c)
	{ return '0' <= c && c <= '7'; }

char Scanner::doesc()
	{
	++si; // backslash
	switch (source[si])
		{
	case 'n' :
		return '\n';
	case 't' :
		return '\t';
	case 'r' :
		return '\r';
	case 'x' :
		if (isxdigit(source[si + 1]) && isxdigit(source[si + 2]))
			{
			si += 2;
			return 16 * hexval(source[si - 1]) + hexval(source[si]);
			}
		else
			return source[--si];
	case '\\' :
	case '"' :
	case '\'' :
		return source[si];
	default : 
		if (isodigit(source[si]) && isodigit(source[si + 1]) && isodigit(source[si + 2]))
			{
			si += 2;
			return (source[si] - '0') + 
				8 * (source[si-1] - '0') + 
				64 * (source[si-2] - '0');
			}
		else
			return source[--si];
		}
	}

struct Keyword
	{
	char* word;
	int id;
	};
static Keyword words[] =
	{
	{ "False", K_FALSE }, { "True", K_TRUE }, 
	{ "and", T_AND }, { "bool", K_BOOL}, { "break", K_BREAK },
	{ "buffer", K_BUFFER }, { "callback", K_CALLBACK },
	{ "case", K_CASE }, { "catch", K_CATCH }, { "char", K_CHAR },
	{ "class", K_CLASS }, { "continue", K_CONTINUE }, 
	{ "default", K_DEFAULT }, { "dll", K_DLL }, { "do", K_DO }, 
	{ "double", K_DOUBLE }, { "else", K_ELSE }, { "false", K_FALSE }, 
	{ "float", K_FLOAT }, { "for", K_FOR }, 
	{ "forever", K_FOREVER }, { "function", K_FUNCTION }, 
	{ "gdiobj", K_GDIOBJ }, { "handle", K_HANDLE }, { "if", K_IF }, 
	{ "in", K_IN }, { "int64", K_INT64 }, { "is", I_IS }, 
	{ "isnt", I_ISNT }, { "long", K_LONG },
	{ "new", K_NEW }, { "not", I_NOT }, { "or", T_OR }, { "pointer", K_POINTER },
	{ "resource", K_RESOURCE },	{ "return", K_RETURN }, 
	{ "short", K_SHORT }, { "string", K_STRING }, { "struct", K_STRUCT }, 
	{ "super", K_SUPER }, { "switch", K_SWITCH }, { "this", K_THIS }, 
	{ "throw", K_THROW }, { "true", K_TRUE }, { "try", K_TRY }, 
	{ "void", K_VOID }, { "while", K_WHILE }, 
	{ "xor", I_ISNT }
	};
const int nwords = sizeof words / sizeof (Keyword);

inline bool lt(char* ss, char* tt)
	{
	unsigned char* s = reinterpret_cast<unsigned char*>(ss);
	unsigned char* t = reinterpret_cast<unsigned char*>(tt);
	for (; *s && *s == *t; ++s, ++t)
		;
	return *s < *t;
	}
struct ScannerLt
	{
	bool operator()(const Keyword& k1, const Keyword& k2)
		{ return lt(k1.word, k2.word); }
	bool operator()(const Keyword& k, char* s)
		{ return lt(k.word, s); }
	bool operator()(char* s, const Keyword& k)
		{ return lt(s, k.word); }
	};

int Scanner::keywords(char* s)
	{
	std::pair<Keyword*,Keyword*> p = std::equal_range(words, words + nwords, s, ScannerLt());
	return p.first < p.second ? p.first->id : 0;
	}

#include "testing.h"

static char* input = "and break case catch continue class callback default dll do \
	else for forever function if is isnt or not \
	new switch struct super return throw try while \
	true True false False \
	== = =~ ~ != !~ ! <<= << <> <= < \
	>>= >> >= > || |= | && &= &  \
	^= ^ -- -= - ++ += + /= / \
	*= * %= % $= $ name _name name123 'string' \
	\"string\" 123 123name .name  Name Name123 name? 1$2 +1 num=1 \
	num+=1 1%2 /*comments*/ //comments";

struct Result
	{
	int token;
	char* value;
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
	{ T_STRING, "string" }, { T_STRING, "string"}, 
	{ T_NUMBER, "123" }, { T_NUMBER, "123" }, 
	{ T_IDENTIFIER, "name" }, { '.', 0 }, 
	{ T_IDENTIFIER, "name" }, { T_IDENTIFIER, "Name" },
	{ T_IDENTIFIER, "Name123" }, { T_IDENTIFIER, "name?" }, 
	{ T_NUMBER, "1" }, { I_CAT, 0 }, { T_NUMBER, "2" }, { I_ADD, 0 }, 
	{ T_NUMBER, "1" }, { T_IDENTIFIER, "num" }, { I_EQ, 0 }, { T_NUMBER, "1" }, 
	{ T_IDENTIFIER, "num" }, { I_ADDEQ, 0 }, { T_NUMBER, "1" }, 
	{ T_NUMBER, "1" }, { I_MOD, 0 }, {T_NUMBER, "2" },
	{ T_STRING, "comment" }
	};

class test_scanner : public Tests
	{
	TEST(1, scan)
		{
		int token;
		Scanner sc(input);
		for (int i = 0; Eof != (token = sc.next()); ++i)
			{
			asserteq(results[i].token,
				(results[i].token < KEYWORDS ? token : sc.keyword));
			if (results[i].value)
				asserteq(0, strcmp(sc.value, results[i].value));
			}
		}
	};
REGISTER(test_scanner);
