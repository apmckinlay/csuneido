// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "auth.h"
#include "htbl.h"
#include "thedb.h"
#include "database.h"
#include <cstdlib> // for srand and rand
#include <ctime>   // for time which is used to seed rand

static Hset<gcstring> tokens;

gcstring rndstr(int size) {
	static bool first = true;
	if (first) {
		srand(time(nullptr));
		first = false;
	}
	char* buf = salloc(size);
	for (int i = 0; i < size; ++i)
		buf[i] = rand();
	return gcstring::noalloc(buf, size);
}

gcstring Auth::nonce() {
	return rndstr(NONCE_SIZE);
}

gcstring Auth::token() {
	gcstring token = rndstr(TOKEN_SIZE);
	tokens.insert(token);
	return token;
}

static bool is_token(const gcstring& data) {
	return tokens.erase(data);
}

const char separator = 0;

extern gcstring sha1(const gcstring& s);

static gcstring getPassHash(const gcstring& user) {
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

static bool is_user(const gcstring& nonce, const gcstring& data) {
	int i = data.find(separator);
	if (i == -1)
		return false;
	gcstring user = data.substr(0, i);
	gcstring hash = data.substr(i + 1);
	gcstring passHash = getPassHash(user);
	gcstring shouldBe = sha1(nonce + passHash);
	return hash == shouldBe;
}

bool Auth::auth(const gcstring& nonce, const gcstring& data) {
	return is_token(data) || is_user(nonce, data);
}

#include "testing.h"

TEST(auth) {
	gcstring t = Auth::token();
	assert_eq(Auth::TOKEN_SIZE, t.size());
	gcstring t2 = Auth::token();
	assert_neq(t, t2);
	verify(is_token(t));
	verify(!is_token(t));
	verify(is_token(t2));
	verify(!is_token(t2));
}
