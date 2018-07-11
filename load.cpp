// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "load.h"
#include "istreamfile.h"
#include "database.h"
#include "thedb.h"
#include "record.h"
#include "query.h"
#include "port.h"
#ifdef _WIN32
#include <io.h> // for access
#else
#include <unistd.h> // for access
#endif
#include "alert.h"
#include "errlog.h"
#include "exceptimp.h"

static char* loadbuf = 0;
static int loadbuf_size = 0;
static int load1(Istream& fin, gcstring tblspec);
static int load_data(Istream& fin, const gcstring& table);
static void load_data_record(Istream& fin, const gcstring& table, int tran, int n);
static bool alerts = false;

struct Loading
	{
	Loading()
		{
		theDB()->loading = true;
		loadbuf_size = 100000;
		loadbuf = (char*) mem_committed(loadbuf_size);
		verify(loadbuf);
		}
	~Loading()
		{
		theDB()->loading = false;
		mem_release(loadbuf);
		loadbuf = 0;
		loadbuf_size = 0;
		}
	};

extern bool thedb_create;

void load(const gcstring& table)
	{
	thedb_create = true;

	if (table != "")							// load a single table
		{
		load_table(table);
		}
	else										// load entire database
		{
		const size_t bufsize = 8000;
		char buf[bufsize];
		IstreamFile fin("database.su", "rb");
		if (! fin)
			except("can't open database.su");
		fin.getline(buf, bufsize);
		if (! has_prefix(buf, "Suneido dump 1"))
			except("invalid file");

		if (_access("suneido.db", 0) == 0)
			{
			remove("suneido.bak");
			verify(0 == rename("suneido.db", "suneido.bak"));
			}

		Loading loading;
		while (fin.getline(buf, bufsize))
			{
			if (has_prefix(buf, "======"))
				{
				memcpy(buf, "create", 6);
				load1(fin, buf);
				}
			else
				except("bad file format");
			}
		verify(!alerts);
		}
	}

int load_table(const gcstring& table)
	{
	const size_t bufsize = 8000;
	char buf[bufsize];
	IstreamFile fin((table + ".su").str(), "rb");
	if (!fin)
		except("can't open " << table << ".su");
	fin.getline(buf, bufsize);
	if (!has_prefix(buf, "Suneido dump 1"))
		except("invalid file");

	char* buf2 = buf + table.size() + 1;
	fin.getline(buf2, bufsize);
	verify(0 == memcmp(buf2, "======", 6));
	memcpy(buf, "create ", 7);
	memcpy(buf + 7, table.ptr(), table.size());
	Loading loading;
	int n = load1(fin, buf);
	verify(!alerts);
	return n;
	}

static int load1(Istream& fin, gcstring tblspec)
	{
	int n = tblspec.find(' ', 7);
	gcstring table = tblspec.substr(7, n - 7);

	if (table != "views")
		{
		if (theDB()->istable(table))
			theDB()->remove_table(table);
		database_admin(tblspec.str());
		}
	return load_data(fin, table);
	}

const int recsPerTran = 50;

static int load_data(Istream& fin, const gcstring& table)
	{
	int nrecs = 0;
	int tran = theDB()->transaction(READWRITE);
	for (;; ++nrecs)
		{
		int n;
		fin.read((char*) &n, sizeof n);
		if (fin.gcount() != sizeof n)
			except("unexpected eof");
		if (n == 0)
			break ;
		load_data_record(fin, table, tran, n);
		if (nrecs % recsPerTran == recsPerTran - 1)
			{
			verify(theDB()->commit(tran));
			tran = theDB()->transaction(READWRITE);
			}
		}
	verify(theDB()->commit(tran));
	return nrecs;
	}

static void load_data_record(Istream& fin, const gcstring& table, int tran, int n)
	{
	try
		{
		if (n > loadbuf_size)
			{
			loadbuf_size = max(n, 2 * loadbuf_size);
			mem_release(loadbuf);
			loadbuf = (char*) mem_committed(loadbuf_size);
			verify(loadbuf);
			}
		fin.read(loadbuf, n);
		Record rec(loadbuf);
		if (rec.cursize() != n)
			except_err(table << ": rec size " << rec.cursize() << " not what was read " << n);
		if (table == "views")
			theDB()->add_any_record(tran, table, rec);
		else
			theDB()->add_record(tran, table, rec);
		}
	catch (const Except& e)
		{
		errlog("load: skipping corrupted record in: ", table.str(), e.str());
		alert("skipping corrupted record in: " << table << ": " << e);
		alerts = true;
		}
	}


