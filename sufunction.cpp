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
#include "sustring.h"
#include "suobject.h"
#include "opcodes.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "ostreamstr.h"
#include "catstr.h"
#include <ctype.h>
#include "varint.h"
#include "suinstance.h"

#define TARGET(i)	(short) ((code[i] + (code[i+1] << 8)))

Value SuFunction::call(Value self, Value member, 
	short nargs, short nargnames, uint16_t* argnames, int each)
	{
	static Value Disasm("Disasm");
	static Value Source("Source");

	if (member == CALL)
		{
		args(nargs, nargnames, argnames, each);

		if (flags)
			dotParams(self);

		Framer frame(this, self);
		return tls().proc->fp->run();
		}
	else if (member == Disasm)
		{
		NOARGS("function.Disasm()");
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
	SuInstance* ob = val_cast<SuInstance*>(self);
	if (! ob)
		return ;
	Value* args = GETSP() - nparams + 1;
	for (int i = 0; i < nparams; ++i)
		if (flags[i] & DOT)
			{
			auto name = symstr(locals[i]);
			if (flags[i] & PUB)
				{
				char* s = STRDUPA(name);
				*s = toupper(*s);
				name = s;
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
	else if (op < 16 || op == I_BOOL)
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
			out << mem(ci) << " " << (op & 7);
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
			out << literals[varint(code, ci)];
			break ;
		case AUTO :
		case DYNAMIC :
			out << symstr(locals[code[ci++]]);
			break ;
		case MEM :
		case MEM_SELF :
			out << mem(ci);
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
			out << mem(ci);
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
			out << " " << literals[varint(code, ci)];
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
			out << mem(ci);
			break ;
		default :
			break ;
			}
		}
	out << "\n";
	return ci;
	}

const char* SuFunction::mem(int& ci)
	{
	int n = varint(code, ci);
	except_if(n >= nliterals, "n " << n << " nliterals " << nliterals);
	return literals[n].str();
	}
