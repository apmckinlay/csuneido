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

#include "auth.h"
#include "hashmap.h"
#include "thedb.h"
#include "database.h"
#include <stdlib.h> // for srand and rand
#include <time.h> // for time which is used to seed rand

static HashMap<gcstring, bool> tokens;

gcstring rndstr(int size)
	{
	static bool first = true;
	if (first)
		{
		srand(time(NULL));
		first = false;
		}
	gcstring s(size);
	char* dst = s.buf();
	for (int i = 0; i < size; ++i)
		*dst++ = rand();
	return s;
	}

gcstring Auth::nonce()
	{
	return rndstr(NONCE_SIZE);
	}

gcstring Auth::token()
	{
	gcstring token = rndstr(TOKEN_SIZE);
	tokens[token] = true;
	return token;
	}

static bool is_token(const gcstring& data)
	{
	return tokens.erase(data);
	}

const char separator = 0;

extern gcstring sha1(const gcstring& s);

static gcstring getPassHash(const gcstring& user)
	{
	Lisp<gcstring> flds = theDB()->get_fields("users");
	int pass_fld = search(flds, "passhash");
	if (pass_fld < 0)
		return "";
	Record key;
	key.addval(user);
	TranCloser t(theDB()->transaction(READONLY));
	Index* index = theDB()->get_index("users", "user");
	Index::iterator iter = index->begin(t, key);
	if (iter.eof())
		return "";
	Record rec(iter.data());
	return rec.getstr(pass_fld);
	}

static bool is_user(const gcstring& nonce, const gcstring& data)
	{
	int i = data.find(separator);
	if (i == -1)
		return false;
	gcstring user = data.substr(0, i);
	gcstring hash = data.substr(i + 1);
	gcstring passHash = getPassHash(user);
	gcstring shouldBe = sha1(nonce + passHash);
	return hash == shouldBe;
	}

bool Auth::auth(const gcstring& nonce, const gcstring& data)
	{
	return is_token(data) || is_user(nonce, data);
	}
