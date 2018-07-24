#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "database.h"
#include <cstdio> // for remove

extern Database* thedb;

class TempDB { // need class to ensure thedb is restored
public:
	explicit TempDB(bool r = true) : rm(r) {
		remove("tempdb");
		savedb = thedb;
		thedb = new Database("tempdb", DBCREATE);
#ifdef BIGDB
		for (int i = 0; i < 1100; ++i)
			thedb->mmf->alloc(4000000, MM_OTHER, false);
#endif
	}
	TempDB(const TempDB&) = delete;
	TempDB& operator=(const TempDB&) = delete;
	TempDB(TempDB&&) = delete;
	TempDB& operator=(const TempDB&&) = delete;

	static void reopen() {
		delete thedb;
		thedb = new Database("tempdb");
	}
	void close() const {
		delete thedb;
		thedb = savedb;
	}

	static void open() {
		thedb = new Database("tempdb");
	}
	~TempDB() {
		close();
		if (rm)
			remove("tempdb");
	}

private:
	Database* savedb;
	bool rm;
};
