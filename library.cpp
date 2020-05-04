// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "library.h"
#include "compile.h"
#include "interp.h"
#include "globals.h"
#include "named.h"
#include "except.h"
#include "exceptimp.h"
#include "sudb.h"
#include "dbms.h"
#include "sustring.h"
#include "catstr.h"
#include "pack.h"
#include "cmdlineoptions.h"
#include "fatal.h"
#include "trace.h"
#include "htbl.h"

// register builtins

Hmap<int, Value> builtins;

void builtin(int gnum, Value value) {
	if (builtins.find(gnum))
		fatal("duplicate builtin:", globals(gnum));
	builtins.put(gnum, value);
}

// local libraries

extern bool is_client;

Dbms* libdb() {
	if (is_client && cmdlineoptions.local_library) {
		static Dbms* local_library_dbms = dbms_local();
		return local_library_dbms;
	} else
		return dbms();
}

// LibraryOverride

Hmap<gcstring, gcstring> override;

// load on demand - called by globals
void libload(int gnum) {
	if (Value* pv = builtins.find(gnum)) {
		globals.put(gnum, *pv);
		return;
	}
	if (cmdlineoptions.action == TEST)
		return; // don't access libraries during built-in tests
	char* gname = globals(gnum);
	if (strcspn(gname, "\r\n") != strlen(gname))
		return;
	Lisp<gcstring> srcs = libdb()->libget(gname);
	for (; !nil(srcs); ++srcs) {
		gcstring lib = *srcs;
		++srcs;
		try {
			const char* src;
			if (gcstring* ps = override.find(lib + ":" + gname))
				src = ps->str();
			else
				src = (*srcs).str();
			Value x = compile(src, gname);
			if (Named* n = const_cast<Named*>(x.get_named())) {
				n->lib = lib;
				n->num = gnum;
			}
			globals.put(gnum, x);
			TRACE(LIBRARIES, "loaded " << gname << " from " << lib);
		} catch (const Except& e) {
			globals.put(gnum, Value());
			except("error loading " << gname << " from " << lib << ": " << e);
		}
	}
}

static Lisp<gcstring> thelibs;

Lisp<gcstring> libraries() {
	if (nil(thelibs))
		thelibs.append("stdlib");
	return thelibs;
}

// builtin functions for libraries

#include "builtin.h"

BUILTIN(Use, "(string)") {
	gcstring lib = TOP().gcstr();
	if (member(libdb()->libraries(), lib))
		return SuFalse;
	if (is_client && !cmdlineoptions.local_library)
		except("can't Use('"
			<< lib << "')\nWhen client-server, only the server can Use");
	TRACE(LIBRARIES, "Use " << lib);
	try {
		DbmsQuery* cursor =
			libdb()->cursor(CATSTRA(lib.str(), " project group, name, text"));
		if (!cursor)
			return SuFalse;
		cursor->close();

		libdb()->admin(CATSTR3("ensure ", lib.str(), " key(name,group)"));
	} catch (const Except& e) {
		except("Use failed: " << e);
	}
	thelibs.append(lib);
	globals.clear();
	return SuTrue;
}

BUILTIN(Unuse, "(string)") {
	gcstring lib = TOP().gcstr();
	if (lib == "stdlib" || !member(libdb()->libraries(), lib))
		return SuFalse;
	if (is_client && !cmdlineoptions.local_library)
		except("can't Unuse('"
			<< lib << "')\nWhen client-server, only the server can Unuse");
	TRACE(LIBRARIES, "Unuse " << lib);
	thelibs.erase(lib);
	globals.clear();
	return SuTrue;
}

#include "suobject.h"

BUILTIN(Libraries, "()") {
	SuObject* ob = new SuObject();
	for (Lisp<gcstring> l = libdb()->libraries(); !nil(l); ++l)
		ob->add(new SuString(*l));
	return ob;
}

BUILTIN(Unload, "(name = false)") {
	if (TOP() == SuFalse) {
		TRACE(LIBRARIES, "Unload()");
		globals.clear();
	} else {
		gcstring name = TOP().gcstr();
		// TODO: check for valid name
		TRACE(LIBRARIES, "Unload " << name);
		globals.put(name.str(), Value());
	}
	return Value();
}

BUILTIN(LibraryOverride, "(lib, name, text = '')") {
	const int nargs = 3;
	gcstring lib = ARG(0).gcstr();
	gcstring name = ARG(1).gcstr();
	gcstring key = lib + ":" + name;
	gcstring text = ARG(2).gcstr();
	if (text == "") {
		if (!override.find(key))
			return Value();
		TRACE(LIBRARIES, "LibraryOverride erase " << lib << ":" << name);
		override.erase(key);
	} else {
		TRACE(LIBRARIES, "LibraryOverride " << lib << ":" << name);
		override.put(key, text);
	}
	globals.put(name.str(), Value());
	return Value();
}

BUILTIN(LibraryOverrideClear, "()") {
	TRACE(LIBRARIES, "LibraryOverrideClear");
	for (auto [key, val] : override) {
		(void) val;
		int i = key.find(':');
		gcstring name = key.substr(i + 1);
		globals.put(name.str(), Value());
	}
	override.clear();
	return Value();
}

#include <cctype>

SuObject* getBuiltinNames() {
	auto c = new SuObject();
	for (auto [key, val] : builtins) {
		(void) val;
		char* name = globals(key);
		if (isupper(*name))
			c->add(name);
	}
	c->sort();
	c->set_readonly();
	return c;
}

BUILTIN(BuiltinNames, "()") {
	static SuObject* list = getBuiltinNames(); // once only
	return list;
}

// low level libget used by dbmslocal

#include "database.h"
#include "thedb.h"

Lisp<gcstring> libgetall(const char* name) {
	// pre: name is a global you want to lookup in the libraries
	// post: returns a list of alternating library names and values
	Record key;
	key.addval(name);
	key.addval(Value(-1));
	Lisp<gcstring> srcs;
	TranCloser t(theDB()->transaction(READONLY));
	for (Lisp<gcstring> libs = reverse(libraries()); !nil(libs); ++libs) {
		Fields flds = theDB()->get_fields(*libs);
		int group_fld = search(flds, "group");
		int text_fld = search(flds, "text");
		Index* index = theDB()->get_index(*libs, "name,group");
		if (group_fld < 0 || text_fld < 0 || index == nullptr)
			continue; // library is invalid, ignore it
		Index::iterator iter = index->begin(t, key);
		if (!iter.eof()) {
			Record rec(iter.data());
			srcs.push(rec.getstr(text_fld));
			srcs.push(*libs);
		}
	}
	return srcs;
}
