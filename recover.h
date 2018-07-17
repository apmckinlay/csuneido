#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "mmoffset.h"
#include <ctime>

class Mmfile;

bool check_shutdown(Mmfile* mmf);

bool null_progress(int);

enum DbrStatus { DBR_OK, DBR_ERROR, DBR_UNRECOVERABLE };

class DbRecover
	{
public:
	virtual ~DbRecover() = default;
	static DbRecover* check(const char* file,
		bool (*progress)(int) = null_progress);
	virtual DbrStatus check_indexes(
		bool (*progress)(int) = null_progress) = 0;
	virtual DbrStatus status() = 0;
	virtual const char* last_good_commit() = 0;
	virtual bool rebuild(bool (*progress)(int) = null_progress, bool log = true) = 0;
	};

struct Commit
	{
	Commit(int tr, int nc, int nd)
		:  cksum(0), t(time(0)), tran(tr), ncreates(nc), ndeletes(nd)
		{ }
	Mmoffset32* creates()
		{ return reinterpret_cast<Mmoffset32*>((char*) this + sizeof (Commit)); }
	Mmoffset32* deletes()
		{ return reinterpret_cast<Mmoffset32*>((char*) this + sizeof (Commit) + ncreates * sizeof (Mmoffset32)); }
	size_t size() const
		{ return sizeof (Commit) + (ncreates + ndeletes) * sizeof (Mmoffset32); }

	uint32_t cksum;
	time_t t;
	int tran;
	int ncreates;
	int ndeletes;
	// followed by list of creates
	// followed by list of deletes
	};

struct Session
	{
	enum Mode { STARTUP, SHUTDOWN };
	Session(Mode m) : mode(m), t(time(0))
		{ }
	Mode mode;
	time_t t;
	};

void dbdump(const char* db = "suneido.db", bool append = false);
