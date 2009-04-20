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

#include "library.h"
#include "compile.h"
#include "interp.h"
#include "globals.h"
#include "named.h"
#include "except.h"
#include "exceptimp.h"
#include "sudb.h"
#include "dbms.h"
#include "suboolean.h"
#include "sustring.h"
#include "catstr.h"
#include "pack.h"
#include "cmdlineoptions.h"
#include "fatal.h"

// register builtins

HashMap<int,Value> builtins;

void builtin(int gnum, Value value)
	{
	if (builtins.find(gnum))
		fatal("duplicate builtin:", globals(gnum));
	builtins[gnum] = value;
	}

// local libraries

extern bool is_client;

Dbms* libdb()
	{
	if (is_client && cmdlineoptions.local_library)
		{
		static Dbms* local_library_dbms = dbms_local();
		return local_library_dbms;
		}
	else
		return dbms();
	}

// load on demand - called by globals

void libload(ushort gnum)
	{
	if (Value* pv = builtins.find(gnum))
		{
		globals.put(gnum, *pv);
		return ;
		}
	char* gname = globals(gnum);
	if (strcspn(gname, "\r\n") != strlen(gname))
		return ;
	Lisp<gcstring> srcs = libdb()->libget(gname);
	for (; ! nil(srcs); ++srcs)
		{
		gcstring lib = *srcs;
		++srcs;
		if (lib == "")
			{ // compiled
			globals.put(gnum, ::unpack(*srcs));
			if (trace_level & TRACE_LIBLOAD)
				tout() << "LIBLOAD " << gname << " (compiled)" << endl;
			break ;
			}
		try
			{
			char* src = unpack_gcstr(*srcs).str();
			Value x = compile(src, gname);
			if (Named* n = x.get_named())
				{
				n->lib = lib;
				n->num = gnum;
				}
			globals.put(gnum, x);
			if (trace_level & TRACE_LIBLOAD)
				tout() << "loaded " << gname << " from " << lib << endl;
			}
		catch (const Except* e)
			{
			except("error loading " << gname << " from " << lib << ": " << e);
			}
		}
	}

static Lisp<gcstring> libs;

Lisp<gcstring> libraries()
	{
	if (nil(libs))
		libs.append("stdlib");
	return libs;
	}

// builtin functions for libraries

#include "prim.h"

Value su_use()
	{
	gcstring lib = TOP().gcstr();
	if (member(libdb()->libraries(), lib))
		return SuFalse;
	if (is_client && ! cmdlineoptions.local_library)
		except("can't Use('" << lib << "')\nWhen client-server, only the server can Use");
	try
		{
		DbmsQuery* cursor = libdb()->cursor(
			CATSTRA(lib.str(), " project group, name, text"));
		if (! cursor)
			return SuFalse;
		cursor->close();

		libdb()->admin(CATSTR3("ensure ", lib.str(), " key(name,group)"));
		}
	catch (...)
		{ 
		return SuFalse; 
		}
	libs.append(lib);
	globals.clear();
	return SuTrue;
	}
PRIM(su_use, "Use(string)");

Value su_unuse()
	{
	gcstring lib = TOP().gcstr();
	if (! member(libdb()->libraries(), lib))
		return SuFalse;
	if (is_client && ! cmdlineoptions.local_library)
		except("can't Unuse('" << lib << "')\nWhen client-server, only the server can Unuse");
	libs.erase(lib);
	globals.clear();
	return SuTrue;
	}
PRIM(su_unuse, "Unuse(string)");

#include "suobject.h"

Value su_libraries()
	{
	SuObject* ob = new SuObject();
	for (Lisp<gcstring> l = libdb()->libraries(); ! nil(l); ++l)
		ob->add(new SuString(*l));
	return ob;
	}
PRIM(su_libraries, "Libraries()");

// low level libget used by dbmslocal

#include "database.h"
#include "thedb.h"

Lisp<gcstring> libgetall(char* name)
	// pre: name is a global you want to lookup in the libraries
	// post: returns a list of alternating library names and values
	{
	Record key;
	key.addval(name);
	key.addval(Value(-1));
	Lisp<gcstring> srcs;
	int t = theDB()->transaction(READONLY);
	for (Lisp<gcstring> libs = reverse(libraries()); ! nil(libs); ++libs)
		{
		Fields flds = theDB()->get_fields(*libs);
		int group_fld = search(flds, "group");
		int text_fld = search(flds, "text");
		int compiled_fld = search(flds, "compiled");
		Index* index = theDB()->get_index(*libs, "name,group");
		if (group_fld < 0 || text_fld < 0 || index == NULL)
			continue ; // library is invalid, ignore it
		Index::iterator iter = index->begin(t, key);
		if (! iter.eof())
			{
			Record rec(iter.data());
			gcstring compiled;
			if (compiled_fld >= 0 && 
				(compiled = rec.getraw(compiled_fld)).size() > 0)
				{
				srcs.push(compiled);
				srcs.push("");
				break ;
				}
			srcs.push(rec.getraw(text_fld).to_heap());
			srcs.push(*libs);
			}
		}
	theDB()->commit(t);
	return srcs;
	}
