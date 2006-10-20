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

#include "symbols.h"
#include "suvalue.h"
#include "permheap.h"
#include "hashtbl.h"
#include "sustring.h"
#include "sunumber.h"
#include "interp.h" // for stack for call
#include "func.h" // for argseach for call
#include "itostr.h"

// unique strings used for member names
class SuSymbol : public SuString
	{
public:
	SuSymbol(const char* s) : SuString(strlen(s), s) // no alloc
		{ }
	virtual int symnum() const;
	virtual bool eq(const SuValue& y) const;
	virtual void out(Ostream& os);
	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	};

const int MAX_SYMBOLS = 32 * 1024;
static PermanentHeap symbols("symbol table", MAX_SYMBOLS * sizeof (SuSymbol));
const int AVG_NAME_LEN = 16;

struct kofv
	{
	const char* operator()(SuSymbol* sym)
		{ return sym->str(); }
	};

static Hashtbl<const char*,SuSymbol*,kofv> symtbl;

int SuSymbol::symnum() const
	{ 
	return 0x8000 | (this - (SuSymbol*) symbols.begin());
	}

bool SuSymbol::eq(const SuValue& y) const
	{
	return symbols.contains(&y) ? this == &y : SuString::eq(y);
	}

void SuSymbol::out(Ostream& os)
	{
	extern bool obout_inkey;
	if (! obout_inkey)
		os << '#';
	if (is_identifier())
		os << gcstr();
	else
		SuString::out(os);
	}

Value SuSymbol::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL)
		{ // #symbol(ob, ...) => ob.symbol(...)
		if (nargs < 1)
			except("usage: #symbol(object, ...)");
		// remove first argument (object) from stack
		argseach(nargs, nargnames, argnames, each); // have to expand first
		Value* args = proc->stack.getsp() - nargs + 1;
		Value ob = args[0];
		for (int i = 1; i < nargs; ++i)
			args[i - 1] = args[i];
		proc->stack.pop();
		return ob.call(ob, this, nargs - 1, nargnames, argnames, each);
		}
	return SuString::call(self, member, nargs, nargnames, argnames, each);
	}

int symnum(const char* s)
	{
	return 0x8000 | (((SuSymbol*) symbol(s).ptr()) - (SuSymbol*) symbols.begin());
	}

Value symbol(const gcstring& s)
	{
	return symbol(s.str());
	}

Value symbol(const char* s)
	{
	static PermanentHeap names("symbol names", MAX_SYMBOLS * AVG_NAME_LEN);

	if (SuSymbol** psym = symtbl.find(s))
		return *psym;
	char* name = strcpy((char*) names.alloc(strlen(s) + 1), s);
	SuSymbol* sym = new(symbols.alloc(sizeof (SuSymbol))) SuSymbol(name);
	symtbl.insert(sym);
	TRACE(SYMBOL, name);
	return sym;
	}

Value symbol(int i)
	{
	if (! (i & 0x8000))
		return i;
	SuSymbol* x = (SuSymbol*) symbols.begin() + (i & 0x7fff);
	verify(symbols.contains(x));
	return x;
	}

char* symstr(int i)
	{
	return i & 0x8000
		? symbol(i).str()
		: itostr(i, new char[8], 10);
	}

// test =============================================================

#include "testing.h"

class test_symbols : public Tests
	{
	TEST(0, main)
		{
		char* syms[] = { "one", "two", "three", "four" };
		const int n = sizeof (syms) / sizeof (char*);
		ushort nums[n];
		int i;
		for (i = 0; i < n; ++i)
			nums[i] = symnum(syms[i]);
		for (i = 0; i < n; ++i)
			asserteq(symbol(nums[i]).gcstr(), syms[i]);
		}
	};
REGISTER(test_symbols);
