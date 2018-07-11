// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "dump.h"
#include "database.h"
#include "thedb.h"
#include "ostreamfile.h"
#include "fibers.h" // for yieldif

static int dump1(OstreamFile& fout, int tran, const gcstring& table, bool output_name = true);

struct Session
	{
	Session()
		{
		tran = theDB()->transaction(READONLY);
		}
	~Session()
		{
		theDB()->commit(tran);
		}
	int tran;
	};

void dump(const gcstring& table)
	{
	Session session;

	if (table != "")
		{
		OstreamFile fout((table + ".su").str(), "wb");
		if (! fout)
			except("can't create " << table + ".su");
		fout << "Suneido dump 1.0" << endl;
		dump1(fout, session.tran, table, false);
		}
	else
		{
		OstreamFile fout("database.su", "wb");
		if (! fout)
			except("can't create database.su");
		fout << "Suneido dump 1.0" << endl;
		for (Index::iterator iter = 
				theDB()->get_index("tables", "tablename")->begin(schema_tran);
			! iter.eof(); ++iter)
			{
			Record r(iter.data());
			gcstring t = r.getstr(T_TABLE);
			if (theDB()->is_system_table(t))
				continue ;
			dump1(fout, session.tran, t);
			}
		dump1(fout, session.tran, "views");
		}
	}

static int dump1(OstreamFile& fout, int tran, const gcstring& table, bool output_name)
	{
	fout << "====== "; // load needs this same length as "create"
	if (output_name)
		fout << table << " ";
	theDB()->schema_out(fout, table);
	fout << endl;

	Lisp<gcstring> fields = theDB()->get_fields(table);
	static gcstring deleted = "-";
	bool squeeze = member(fields, deleted);

	int nrecs = 0;
	Index* idx = theDB()->first_index(table);
	verify(idx);
	for (Index::iterator iter = idx->begin(tran);
		! iter.eof(); ++iter, ++nrecs)
		{
		Fibers::yieldif();
		Record rec(iter.data());
		if (squeeze)
			{
			Record newrec(rec.cursize());
			int i = 0;
			for (Lisp<gcstring> f = fields; ! nil(f); ++f, ++i)
				if (*f != "-")
					newrec.addraw(rec.getraw(i));
			rec = newrec.dup();
			}
		int n = rec.cursize();
		fout.write(&n, sizeof n);
		fout.write(rec.ptr(), n);
		}
	int zero = 0;
	fout.write(&zero, sizeof zero);
	return nrecs;
	}
