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

#include "dbserverdata.h"
#include "hashmap.h"
#include "lisp.h"
#include "dbms.h"

int cursors_inuse = 0;

struct TranQuery
	{
	TranQuery() : query(0)
		{ }
	TranQuery(int t, DbmsQuery* q) : tran(t), query(q)
		{ }
	~TranQuery()
		{ if (query) query->close(); }
	int tran;
	DbmsQuery* query;
	};

class DbServerDataImp : public DbServerData
	{
public:
	DbServerDataImp() : nextQnum(0), nextCnum(0)
		{ 
		auth = true; //TEMPORARY  until applications are converted
		}

	void add_tran(int tran);
	int add_query(int tran, DbmsQuery* q);
	DbmsQuery* get_query(int qn);
	bool erase_query(int qn);
	Lisp<int> get_trans();
	void end_transaction(int tran);

	int add_cursor(DbmsQuery* q);
	DbmsQuery* get_cursor(int cn);
	bool erase_cursor(int cn);

	void abort(void (*fn)(int tran));
private:
	int nextQnum;
	int nextCnum;
	HashMap<int,TranQuery> queries;
	HashMap<int,DbmsQuery*> cursors;
	HashMap<int,Lisp<int> > trans; // tran# => list of query#
	};

void DbServerDataImp::add_tran(int tran)
	{
	trans[tran] = Lisp<int>();
	}

int DbServerDataImp::add_query(int tran, DbmsQuery* q)
	{
	queries[nextQnum] = TranQuery(tran, q);
	trans[tran].push(nextQnum);
	return nextQnum++;
	}

DbmsQuery* DbServerDataImp::get_query(int qn)
	{
	TranQuery* tq = queries.find(qn);
	return tq ? tq->query : 0;
	}

bool DbServerDataImp::erase_query(int qn)
	{
	return queries.erase(qn);
	}

Lisp<int> DbServerDataImp::get_trans()
	{
	Lisp<int> t;
	for (HashMap<int,Lisp<int> >::iterator iter = trans.begin();
		iter != trans.end(); ++iter)
		t.append(iter->key);
	return t;
	}

void DbServerDataImp::end_transaction(int tran)
	{
	for (Lisp<int> q = trans[tran]; ! nil(q); ++q)
		queries.erase(*q); // may fail if query was already closed
	// WARNING: this depends on erase calling destructor to close query
	verify(trans.erase(tran));
	}

int DbServerDataImp::add_cursor(DbmsQuery* q)
	{
	++cursors_inuse;
	cursors[nextCnum] = q;
	return nextCnum++;
	}

DbmsQuery* DbServerDataImp::get_cursor(int cn)
	{
	DbmsQuery** pq = cursors.find(cn);
	return pq ? *pq : 0;
	}

bool DbServerDataImp::erase_cursor(int cn)
	{
	--cursors_inuse;
	return cursors.erase(cn);
	}

#include "errlog.h"

void DbServerDataImp::abort(void (*fn)(int tran))
	{
	for (HashMap<int,TranQuery>::iterator qi = queries.begin(); qi != queries.end(); ++qi)
		qi->val.~TranQuery();
	for (HashMap<int,DbmsQuery*>::iterator ci = cursors.begin(); ci != cursors.end(); ++ci)
		{
		ci->val->close();
		--cursors_inuse;
		}
	for (HashMap<int,Lisp<int> >::iterator ti = trans.begin(); ti != trans.end(); ++ti)
		fn(ti->key);
	}

DbServerData* DbServerData::create()
	{
	return new DbServerDataImp;
	}
