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

#include "dll.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "ostreamfile.h"
#include "win.h"
#include "winlib.h"
#include "trace.h"

static WinLib& loadlib(char* name)
	{
	const int NLIBS = 30;
	static WinLib libs[NLIBS];
	static int nlibs = 0;
	int i = 0;
	for (; i < nlibs; ++i)
		if (0 == stricmp(libs[i].name, name))
			return libs[i];
	if (nlibs >= NLIBS)
		except("can't load " << name << " - too many dll's loaded");
	new (libs + nlibs++) WinLib(name);
	return libs[i];
	}

Dll::Dll(short rt, char* library, char* name, TypeItem* p, ushort* ns, short n)
	: params(p,n), rtype(rt), trace(false)
	{
	nparams = n;
	dll = globals(library);
	fn = globals(name);
	locals = dup(ns, nparams);

	WinLib& hlibrary = loadlib(library);
	if (! hlibrary)
		except("can't LoadLibrary " << library);
	if (0 == (pfn = hlibrary.GetProcAddress(name)))
		{
		char uname[64];
		verify(strlen(name) < 63);
		strcpy(uname, name);
		strcat(uname, "A");
		if (0 == (pfn = hlibrary.GetProcAddress(uname)))
			except("can't get " << library << ":" << name << " or " << uname);
		}
	}

void Dll::out(Ostream& os)
	{
	if (named.num)
		os << named.name() << " /* " << named.lib << " dll " << pfn << " */";
	else
		{
		os << "dll " << (rtype ? globals(rtype) : "void") << " " << globals(dll) << ":" << globals(fn);
		os << '(';
		for (int i = 0; i < nparams; ++i)
			{
			if (i > 0)
				os << ", ";
			params.items[i].out(os);
			verify(locals);
			os << " " << symstr(locals[i]);
			}
		os << ')';
		}
	}

static OstreamFile& log()
	{
	static OstreamFile ofs("dll.log");
	return ofs;
	}

const int maxbuf = 1024;

Value Dll::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Trace("Trace");
	if (member == Trace)
		{
		trace = true;
		return Value();
		}
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	args(nargs, nargnames, argnames, each);
	Framer frame(this, self);

	char buf[maxbuf];
	char* dst = buf;
	char buf2[32000];	// for stuff passed by pointer
	char* dst2 = buf2;
	char* lim2 = buf2 + sizeof buf2;

	const int params_size = params.size();
	if (params_size > maxbuf)
		except("dll arguments too big");
	Value* args = GETSP() - nparams + 1;
	params.putall(dst, dst2, lim2, args);
	verify(dst == buf + params_size);

	if (trace)
		{
		log() << named.name() << endl << "    ";
		for (void** p = (void**) buf; p < (void**) dst; ++p)
			log() << *p << " ";
		log() << endl << "    ";
		for (char* s = buf2; s < dst2; ++s)
			log() << hex << (ulong)(uchar) *s << dec << " ";
		log() << endl;
		}

	short save_trace_level = trace_level;
	TRACE_CLEAR();

	long result, result2;
	long nlongs = (dst - buf) / 4;
#ifdef _MSC_VER
	if (nlongs)
		__asm
			{
			mov ecx, nlongs
			mov ebx, dst
	NEXT:	sub ebx,4
			push dword ptr [ebx]
			loop NEXT
			}
	ulong fn = (ulong) pfn;
	__asm
		{
		mov eax,fn
		call eax
		mov result,eax
		mov result2,edx
		}
#elif defined(__GNUC__)
	int dummy;
	if (nlongs)
		asm volatile (
			"1:\n\t"
			"sub $4, %%ebx\n\t"
			"pushl (%%ebx)\n\t"
			"loop 1b\n\t"
			: "=b" (dummy) // outputs
			: "c" (nlongs), "b" (dst) // inputs
			);
	asm volatile (
		"call *%%eax\n\t"
		: "=a" (result), "=d" (result2) // outputs
		: "a" (pfn) // inputs
		: "memory" // clobbers
		);
	// WARNING: should be more clobbers but not sure what???
#else
#warning "replacement for inline assembler required"
#endif

	trace_level = save_trace_level;

	if (trace)
		{
		log() << "    => " << hex << result << dec << endl;
		for (char* s = buf2; s < dst2; ++s)
			log() << hex << (ulong)(uchar) *s << dec << " ";
		}

	// update SuObject args passed by pointer
	char* src = buf;
	params.getall(src, args);

	return rtype ? force<Type*>(globals[rtype])->result(result2, result) : Value();
	}
