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
#include "htbl.h"
#include "sustring.h"
#include "itostr.h"
#include "trace.h"

/*
 * Symbols are shared unique strings used primarily for member names.
 * Symbols are represented by short (16 bit) integers, with the high bit set
 * which means there is a limit of 32k symbols.
 * The strings themselves are stored in a private PermanentHeap (names).
 * SuSymbol inherits from SuString.
 * SySymbol overrides eq to take advantage of uniqueness for performance.
 * The SuSymbol's are stored in another private PermanentHeap (symbols).
 * The index of the SuSymbol in the symbols array is its symbol number.
 */

class SuSymbol : public SuString
	{
public:
	explicit SuSymbol(const char* s) : SuString(gcstring::noalloc(s))
		{ }
	int symnum() const override;
	bool eq(const SuValue& y) const override;
	};

const int MAX_SYMBOLS = 32 * 1024;
static PermanentHeap symbols("symbol table", MAX_SYMBOLS * sizeof (SuSymbol));
const int NAMES_SPACE = 512 * 1024;
static PermanentHeap names("symbol names", NAMES_SPACE);

template<> struct HashFn<SuSymbol*>
	{
	size_t operator()(SuSymbol* k) const
		{ return hashfn(k->str()); }
	size_t operator()(const char* s) const
		{ return hashfn(s); }
	};
template<> struct KeyEq<SuSymbol*>
	{
	bool operator()(const char* x, SuSymbol* y) const
		{ return 0 == strcmp(x, y->str()); }
	bool operator()(SuSymbol* x, SuSymbol* y) const
		{ return 0 == strcmp(x->str(), y->str()); 	}
	};

// The hash set of SuSymbol* used to map char* => SuSymbol
static Hset<SuSymbol*> symtbl(3000); // enough to open IDE

int SuSymbol::symnum() const
	{
	return 0x8000 | (this - (SuSymbol*) symbols.begin());
	}

bool SuSymbol::eq(const SuValue& y) const
	{
	if (this == &y)
		return true;
	return symbols.contains(this) && symbols.contains(&y)
		? false : SuString::eq(y);
	}

int symnum(const char* s)
	{
	return 0x8000 | (((SuSymbol*) symbol(s).ptr()) - (SuSymbol*) symbols.begin());
	}

Value symbol(const gcstring& s)
	{
	return symbol(s.str());
	}

Value symbol_existing(const char* s)
	{
	if (SuSymbol** psym = symtbl.find(s))
		return *psym;
	else
		return Value();
	}

Value symbol(const char* s)
	{
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

const char* symstr(int i)
	{
	return i & 0x8000
		? symbol(i).str()
		: itostr(i, salloc(8), 10);
	}

#include "prim.h"
#include "ostreamstr.h"

Value su_syminfo()
	{
	int n_symbols = symbols.size() / sizeof (SuSymbol);
	OstreamStr os;
	os << "Symbols: Count " << n_symbols << " (max " << MAX_SYMBOLS << "), " <<
		"Size " << names.size() << " (max " << NAMES_SPACE << ")";
	return new SuString(os.str());
	}
PRIM(su_syminfo, "SymbolsInfo()");

#include "ostreamfile.h"

Value su_symdump()
	{
	OstreamFile f("symbols.txt");
	for (SuSymbol* ss = (SuSymbol*) symbols.begin();
		ss < (SuSymbol*) symbols.end(); ++ss)
		f << ss->gcstr() << endl;
	return Value();
	}
PRIM(su_symdump, "DumpSymbols()");

// test =============================================================

#include "testing.h"

class test_symbols : public Tests
	{
	TEST(0, main)
		{
		const char* strs[] = { "one", "two", "three", "four" };
		const int n = sizeof (strs) / sizeof (char*);
		ushort nums[n];
		for (int i = 0; i < n; ++i)
			nums[i] = symnum(strs[i]);

		for (int i = 0; i < n; ++i)
			{
			assert_eq(nums[i], symnum(strs[i]));
			verify(symbol_existing(strs[i]));
			verify(0 == strcmp(strs[i], symstr(nums[i])));
			Value sym = symbol(nums[i]);
			asserteq(sym.gcstr(), strs[i]);
			verify(sym.sameAs(symbol(strs[i])));
			}
		}
	TEST(1, empty_string)
		{
		auto s = _strdup("");
		auto t = _strdup("");

		Value x = symbol(s);
		Value y = symbol(t);
		assert_eq(x.ptr(), y.ptr());
		}
	};
REGISTER(test_symbols);
