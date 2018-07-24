// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "thedb.h"
#include "database.h"
#include "port.h"

bool thedb_create = false;
Database* thedb = 0;

void close_db() {
	delete thedb;
	thedb = nullptr;
}

struct CloseDB {
	~CloseDB() {
		close_db();
	}
};

Database* theDB() {
	static CloseDB closeDB; // static so destroyed on exit

	if (!thedb) {
		thedb = new Database("suneido.db", thedb_create);
		sync_timer(thedb->mmf);
	}
	return thedb;
}

TranCloser::~TranCloser() {
	theDB()->abort(t);
}
