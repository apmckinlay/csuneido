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

#include "sufunction.h"
#include "compile.h"
#include "sustring.h"
#include "suobject.h"
#include "opcodes.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "ostreamstr.h"
#include "cvt.h"
#include "pack.h"
#include "catstr.h"
#include <ctype.h>

#define TARGET(i)	(short) ((code[i] + (code[i+1] << 8)))

Value SuFunction::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Disasm("Disasm");
	static Value Source("Source");

	if (member == CALL)
		{
		args(nargs, nargnames, argnames, each);

		if (flags)
			dotParams(self);

		Framer frame(this, self);
		return tss_proc()->fp->run();
		}
	else if (member == Disasm)
		{
		if (nargs != 0)
			except("usage: function.Disasm()");
		OstreamStr oss;
		disasm(oss);
		return new SuString(oss.str());
		}
	else if (member == Source)
		{
		return new SuString(src);
		}
	else
		return Func::call(self, member, nargs, nargnames, argnames, each);
	}

void SuFunction::dotParams(Value self)
	{
	SuObject* ob = val_cast<SuObject*>(self);
	if (! ob)
		return ;
	Value* args = GETSP() - nparams + 1;
	for (int i = 0; i < nparams; ++i)
		if (flags[i] & DOT)
			{
			char* name = symstr(locals[i]);
			if (flags[i] & PUB)
				{
				name = STRDUPA(name);
				*name = toupper(*name);
				}
			else // private
				name = CATSTR3(className, "_", name);
			ob->putdata(name, args[i]);
			}
	}

int SuFunction::source(int ci, int* pn)
	{
	for (int di = 1; di < nd; ++di)
		if (ci <= db[di].ci)
			{
			int i = db[di - 1].si;
			int j = db[di].si;
			while (j > i && strchr(" \t\r\n};", src[j - 1]))
				--j;
			*pn = j - i;
			return i;
			}
	*pn = 0;
	return 0;
	}

void SuFunction::source(Ostream& out, int ci)
	{
	for (int di = 1; di < nd; ++di)
		if (ci <= db[di].ci)
			{
			int si = db[di - 1].si;
			int line = 1;
			for (int i = 0; i < si; ++i)
				if (src[i] == '\n')
					++line;
			out << line << ": "; 
			for (; si < db[di].si && src[si]; ++si)
				out << (src[si] == '\n' || src[si] == '\r' ? ' ' : src[si]);
			out << endl;
			break ;
			}
	}

void SuFunction::disasm(Ostream& out)
	{
	int di, si, ci;

	for (si = db[0].si, ci = 0, di = 1; di < nd; ++di)
		{
		// output source
		while (si < db[di].si && (src[si] == '\n' || src[si] == '\r'))
			++si;
		for (; si < db[di].si && src[si]; ++si)
			out << ((src[si] == '\n' || src[si] == '\r') ? ' ' : src[si]);
		out << '\n';

		// output code
		while (ci < db[di].ci)
			ci = disasm1(out, ci);
		}
	if (ci > 0)
		out << "\t\t\t\t\t" << setw(3) << ci << "\n";
	}

int SuFunction::disasm1(Ostream& out, int ci)
	{
	verify(locals);
	verify(literals);
	out << "\t\t\t\t\t" << setw(3) << ci << "  ";
	short op = code[ci++];
//	out << hex << setw(3) << op << " " << dec;
	out << opcodes[op] << " ";
	if (op == I_SUPER)
		{
		out << globals(TARGET(ci));
		ci += 2;
		}
	else if (op == I_EACH)
		out << (int) code[ci++];
	else if (op == I_BLOCK)
		{
		out << ci + 2 + TARGET(ci);
		ci += 2;
		int first = code[ci++];
		int nargs = code[ci++];
		for (int i = 0; i < nargs; ++i)
			out << " " << 1 + symstr(locals[first + i]);
		}
	else if (op == I_PUSH_INT)
		{
		out << TARGET(ci);
		ci += 2;
		}
	else if (op < 16)
		;
	else if (op < I_PUSH)
		{
		switch (op & 0xf0)
			{
		case I_PUSH_LITERAL :
			out << literals[op & 15];
			break ;
		case I_PUSH_AUTO :
			out << symstr(locals[op & 15]);
			break ;
		case I_EQ_AUTO :
		case I_EQ_AUTO_POP :
			out << symstr(locals[op & 7]);
			break ;
		case I_CALL_GLOBAL :
			out << globals(TARGET(ci)) << " " << (op & 7);
			ci += 2;
			break ;
		case I_CALL_MEM :
		case I_CALL_MEM_SELF :
			out << symstr(TARGET(ci)) << " " << (op & 7);
			ci += 2;
			break ;
		default : 
			break ;
			}
		}
	else if ((op & 0xf8) == I_PUSH)
		{
		switch (op & 7)
			{
		case LITERAL :
			out << literals[code[ci++]];
			break ;
		case AUTO :
		case DYNAMIC :
			out << symstr(locals[code[ci++]]);
			break ;
		case MEM :
		case MEM_SELF :
			out << symstr(TARGET(ci));
			ci += 2;
			break ;
		case GLOBAL :
			out << globals(TARGET(ci));
			ci += 2;
			break;
		default : 
			break ;
			}
		}
	else if ((op & 0xf8) == I_CALL)
		{
		switch (op & 7)
			{
		case AUTO :
		case DYNAMIC :
			out << symstr(locals[code[ci++]]);
			break ;
		case MEM :
		case MEM_SELF :
			out << symstr(TARGET(ci));
			ci += 2;
			break ;
		case GLOBAL :
			out << globals(TARGET(ci));
			ci += 2;
			break;
		default : 
			break ;
			}
		out << " " << (short) code[ci++];
		short nargnames = code[ci++];
		out << " " << nargnames;
		for (int i = 0; i < nargnames; ++i)
			{
			out << " " << symstr(TARGET(ci));
			ci += 2;
			}
		}
	else if ((op & 0xf8) == I_JUMP || op == I_TRY || op == I_CATCH)
		{
		out << ci + 2 + TARGET(ci);
		ci += 2;
		if (op == I_TRY)
			out << " " << literals[code[ci++]];
		}
	else if (I_ADDEQ <= op && op < I_ADD)
		{
		switch ((op & 0x70) >> 4)
			{
		case AUTO :
		case DYNAMIC :
			out << symstr(locals[code[ci++]]);
			break ;
		case MEM :
		case MEM_SELF :
			out << symstr(TARGET(ci));
			ci += 2;
			break ;
		default : 
			break ;
			}
		}
	out << "\n";
	return ci;
	}

static int namerefs(uchar* code, int nc, char* buf = 0);

size_t SuFunction::packsize() const
	{
	int i;

	int n = 1; // PACK_FUNCTION
	n += packnamesize(named);
	n += 4; // int nc;
	n += nc;
	n += namerefs(code, nc);
	n += 2; // short nliterals;
	for (i = 0; i < nliterals; ++i)
		n += sizeof (long) + literals[i].packsize();
	n += 2; // short nlocals;
	for (i = 0; i < nlocals; ++i)
		n += strlen(symstr(locals[i])) + 1;
	n += 2; // short nparams;
	n += 1; // bool rest;
	n += 2; // short ndefaults;
	return n;
	}

int packsymsize(int i)
	{
	if (i & 0x8000)
		return strlen(symstr(i)) + 1;
	else
		return 3;
	}

int packsym(char* buf, int i)
	{
	if (i & 0x8000)
		return packstr(buf, symstr(i));
	else
		{
		buf[0] = 0;
		cvt_short(buf + 1, i);
		return 3;
		}
	}

int unpacksym(const char*& buf)
	{
	if (*buf)
		return symnum(unpackstr(buf));
	else
		{
		buf += 3;
		return cvt_short(buf - 2);
		}
	}

void SuFunction::pack(char* buf) const
	{
	int i;

	*buf++ = PACK_FUNCTION;

	buf += packname(buf, named);

	cvt_long(buf, nc);
	buf += sizeof (long);
	memcpy(buf, code, nc);
	buf += nc;

	buf += namerefs(code, nc, buf);

	cvt_short(buf, nliterals);
	buf += sizeof (short);
	for (i = 0; i < nliterals; ++i)
		buf += packvalue(buf, literals[i]);

	cvt_short(buf, nlocals);
	buf += sizeof (short);
	for (i = 0; i < nlocals; ++i)
		buf += packstr(buf, symstr(locals[i]));

	cvt_short(buf, nparams);
	buf += sizeof (short);
	*buf++ = (char) rest;
	cvt_short(buf, ndefaults);
	buf += sizeof (short);
	}

#define GLOBAL_REF \
	{ char* s = globals(TARGET(ci)); \
	if (buf) buf += packstr(buf, s); \
	else nn += packstrsize(s); \
	ci += 2; }
#define SYMBOL_REF \
	{ int sym = TARGET(ci); \
	if (buf) buf += packsym(buf, sym); \
	else nn += packsymsize(sym); \
	ci += 2; }

static int namerefs(uchar* code, int nc, char* buf)
	{
	char* start = buf;
	int nn = 0;
	for (int ci = 0; ci < nc; )
		{
		uchar op = code[ci++];
		if (op == I_SUPER)
			GLOBAL_REF
		else if (op == I_EACH)
			++ci;
		else if (op == I_BLOCK)
			ci += 4;
		else if (op == I_PUSH_INT)
			ci += 2;
		else if (op < 16)
			;
		else if (op < I_PUSH)
			{
			switch (op & 0xf0)
				{
			case I_PUSH_LITERAL :
			case I_PUSH_AUTO :
			case I_EQ_AUTO :
			case I_EQ_AUTO_POP :
				break ;
			case I_CALL_GLOBAL :
				GLOBAL_REF
				break ;
			case I_CALL_MEM :
			case I_CALL_MEM_SELF :
				SYMBOL_REF
				break ;
			default : 
				break ;
				}
			}
		else if ((op & 0xf8) == I_PUSH)
			{
			switch (op & 7)
				{
			case LITERAL :
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				SYMBOL_REF
				break ;
			case GLOBAL :
				GLOBAL_REF
				break;
			default : 
				break ;
				}
			}
		else if ((op & 0xf8) == I_CALL)
			{
			switch (op & 7)
				{
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				SYMBOL_REF
				break ;
			case GLOBAL :
				GLOBAL_REF
				break;
			default : 
				break ;
				}
			++ci;
			int nargnames = code[ci++];
			for (int i = 0; i < nargnames; ++i)
				SYMBOL_REF
			}
		else if ((op & 0xf8) == I_JUMP || op == I_TRY || op == I_CATCH)
			{
			ci += 2;
			if (op == I_TRY)
				++ci;
			}
		else if (I_ADDEQ <= op && op < I_ADD)
			{
			switch ((op & 0x70) >> 4)
				{
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				SYMBOL_REF
				break ;
			default : 
				break ;
				}
			}
		}
	if (buf)
		{
		nn = buf - start;
		verify(nn <= SHRT_MAX);
		}
	return nn;
	}

static int fixnames(uchar* code, int nc, const char* buf);

SuFunction* SuFunction::unpack(const gcstring& s)
	{
	SuFunction* fn = new SuFunction;
	const char* buf = s.buf() + 1; // skip PACK_FUNCTION
	int i;

	buf += unpackname(buf, fn->named);

	fn->nc = cvt_long(buf);
	buf += sizeof (long);
	fn->code = new uchar[fn->nc];
	memcpy(fn->code, buf, fn->nc);
	buf += fn->nc;

	buf += fixnames(fn->code, fn->nc, buf);

	fn->nliterals = cvt_short(buf);
	buf += sizeof (short);
	fn->literals = new Value[fn->nliterals];
	for (i = 0; i < fn->nliterals; ++i)
		fn->literals[i] = unpackvalue(buf);

	fn->nlocals = cvt_short(buf);
	buf += sizeof (short);
	fn->locals = new ushort[fn->nlocals];
	for (i = 0; i < fn->nlocals; ++i)
		{
		fn->locals[i] = ::symnum(buf);
		buf += strlen(buf) + 1;
		}

	fn->nparams = cvt_short(buf);
	buf += sizeof (short);
	fn->rest = !! *buf++;
	fn->ndefaults = cvt_short(buf);
	buf += sizeof (short);

	verify(buf == s.end());
	return fn;
	}

#define FIX_GLOBAL \
	{ ushort n = globals(unpackstr(buf)); \
	code[ci++] = n & 255; \
	code[ci++] = n >> 8; }
#define FIX_SYMBOL \
	{ ushort n = unpacksym(buf); \
	code[ci++] = n & 255; \
	code[ci++] = n >> 8; }

static int fixnames(uchar* code, int nc, const char* buf)
	{
	const char* start = buf;
	for (int ci = 0; ci < nc; )
		{
		uchar op = code[ci++];
		if (op == I_SUPER)
			FIX_GLOBAL
		else if (op == I_EACH)
			++ci;
		else if (op == I_BLOCK)
			ci += 4;
		else if (op == I_PUSH_INT)
			ci += 2;
		else if (op < 16)
			;
		else if (op < I_PUSH)
			{
			switch (op & 0xf0)
				{
			case I_PUSH_LITERAL :
			case I_PUSH_AUTO :
			case I_EQ_AUTO :
			case I_EQ_AUTO_POP :
				break ;
			case I_CALL_GLOBAL :
				FIX_GLOBAL
				break ;
			case I_CALL_MEM :
			case I_CALL_MEM_SELF :
				FIX_SYMBOL
				break ;
			default : 
				break ;
				}
			}
		else if ((op & 0xf8) == I_PUSH)
			{
			switch (op & 7)
				{
			case LITERAL :
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				FIX_SYMBOL
				break ;
			case GLOBAL :
				FIX_GLOBAL
				break;
			default : 
				break ;
				}
			}
		else if ((op & 0xf8) == I_CALL)
			{
			switch (op & 7)
				{
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				FIX_SYMBOL
				break ;
			case GLOBAL :
				FIX_GLOBAL
				break;
			default : 
				break ;
				}
			++ci;
			int nargnames = code[ci++];
			for (int i = 0; i < nargnames; ++i)
				FIX_SYMBOL
			}
		else if ((op & 0xf8) == I_JUMP || op == I_TRY || op == I_CATCH)
			{
			ci += 2;
			if (op == I_TRY)
				++ci;
			}
		else if (I_ADDEQ <= op && op < I_ADD)
			{
			switch ((op & 0x70) >> 4)
				{
			case AUTO :
			case DYNAMIC :
				++ci;
				break ;
			case MEM :
			case MEM_SELF :
				FIX_SYMBOL
				break ;
			default : 
				break ;
				}
			}
		}
	return buf - start;
	}

#include "testing.h"

class test_packfunc : public Tests
	{
	TEST(0, main)
		{
		Value fn = compile("function (a, b, c = 0, d = False) { return A + B() + x + y + 123 + 456 }", "test_packfunc");
		verify(val_cast<SuFunction*>(fn));
		int n = fn.packsize();
		char* buf1 = new char[n + 1];
		buf1[n] = '\xc4';
		fn.pack(buf1);
		verify(buf1[n] == '\xc4');
		Value x = unpack(gcstring(n, buf1));
		verify(val_cast<SuFunction*>(x));
		assert_eq(n, x.packsize());
		char* buf2 = new char[n + 1];
		buf2[n] = '\xc4';
		x.pack(buf2);
		verify(buf2[n] == '\xc4');
		verify(0 == memcmp(buf1, buf2, n));
		}
	};
REGISTER(test_packfunc);
