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

#include "win.h"
#include "scintilla.h"
#include "interp.h"
#include "suvalue.h"
#include "qscanner.h"
#include "minmax.h"
#include <malloc.h> // for alloca
#include "prim.h"

//#define LOGGING
#ifdef LOGGING
#include "ostreamcon.h"
Ostream& log()
	{
	static OstreamCon osc;
	return osc;
	}
#define LOG(stuff) log() << stuff << endl
#else
#define LOG(stuff)
#endif

enum
	{
	STYLE_NORMAL = 0,
	STYLE_COMMENT = 1,
	STYLE_NUMBER = 2,
	STYLE_STRING = 3,
	STYLE_KEYWORD = 4,
	STYLE_OP_PUNCT = 5,
	STYLE_WHITESPACE = 6
	};

static int tokenStyle(int token, int keyword, bool query);

Value su_ScintillaStyle()
	{
	const int nargs = 4;
	HWND hwnd = (HWND) ARG(0).integer();
	int start = ARG(1).integer();
	int end = ARG(2).integer();
	bool query = ARG(3) == SuTrue;
	LOG("style " << start << " -> " << end << " query " << (query ? "true" : "false"));

	int lengthDoc = SendMessage(hwnd, SCI_GETLENGTH, 0, 0);
	if (end == 0 || end > lengthDoc)
		end = lengthDoc;

	// backup to start of previous line
	int line = SendMessage(hwnd, SCI_LINEFROMPOSITION, start, 0);
	LOG("line " << line);
	if (start > 0 && line > 0)
		{
		--line;
		start = SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
		}
	LOG("previous line start " << start);


	// backup to start of line
	line = SendMessage(hwnd, SCI_LINEFROMPOSITION, start, 0);
	start = SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
	LOG("start of line " << start);

	// backup to outside string or comment
	while (start > 0)
		{
		int style = SendMessage(hwnd, SCI_GETSTYLEAT, start, 0);
		if (style != STYLE_COMMENT && style != STYLE_STRING)
			break ;
		start = SendMessage(hwnd, SCI_POSITIONFROMLINE, --line, 0);
		}
	LOG("outside " << start);

	int size = end - start;

	int level = SendMessage(hwnd, SCI_GETFOLDLEVEL, line, 0) & SC_FOLDLEVELNUMBERMASK;
	int prev_level = level;
	LOG("line " << line << " level " << level - SC_FOLDLEVELBASE);

	TextRange tr;
	tr.lpstrText = (char*) alloca(lengthDoc - start + 1), // allow for nul
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = lengthDoc;
	SendMessage(hwnd, SCI_GETTEXTRANGE, 0, (long) &tr);

	char* styles = (char*) alloca(size);

	Scanner* scan = query ? new QueryScanner(tr.lpstrText) : new Scanner(tr.lpstrText);
	SendMessage(hwnd, SCI_STARTSTYLING, start, 0x1f);
	int token;
	while (-1 != (token = scan->nextall()))
		{
		int last = min(scan->si, size);
		int len = last - scan->prev;
		int style = tokenStyle(token, scan->keyword, query);
		LOG("len " << len << " token " << token << " keyword " << scan->keyword << " => style " << style);
		verify(scan->prev + len <= size);
		if (len > 0)
			memset(styles + scan->prev, style, len);

		if (token == '{')
			++level;
		else if (token == '}')
			--level;
		else if (token == T_NEWLINE)
			{
			int i = line;
			line = SendMessage(hwnd, SCI_LINEFROMPOSITION, start + scan->si, 0);
			int flags = level > prev_level ? SC_FOLDLEVELHEADERFLAG : 0;
			for (; i < line; ++i)
				{
				LOG("setfoldlevel " << i << " = " << prev_level - SC_FOLDLEVELBASE << (flags ? " header" : ""));
				SendMessage(hwnd, SCI_SETFOLDLEVEL, i, prev_level | flags);
				flags = 0; // header only on first
				prev_level = level;
				}
			}

		if (scan->si > size)
			break ;
		}
	int i = line;
	line = SendMessage(hwnd, SCI_LINEFROMPOSITION, start + scan->si, 0);
	for (; i <= line; ++i)
		{
		LOG("final setfoldlevel " << i << " = " << prev_level - SC_FOLDLEVELBASE);
		SendMessage(hwnd, SCI_SETFOLDLEVEL, i, prev_level);
		prev_level = level;
		}

	SendMessage(hwnd, SCI_SETSTYLINGEX, size, (long) styles);
	return Value();
	}
PRIM(su_ScintillaStyle, "ScintillaStyle(hwnd, start, end = 0, query = false)");

static int tokenStyle(int token, int keyword, bool query)
	{
	if (keyword > 0 && (query || (keyword != K_LIST && keyword != K_VALUE)))
		return STYLE_KEYWORD;
	switch (token)
		{
	case T_COMMENT :	return STYLE_COMMENT;
	case T_NUMBER :	return STYLE_NUMBER;
	case T_STRING :		return STYLE_STRING;
	case T_WHITE :		return STYLE_WHITESPACE;
	default : return token < 1000 
						? STYLE_OP_PUNCT
						: STYLE_NORMAL;
		}
	}
