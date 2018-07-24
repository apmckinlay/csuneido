// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "dbserverdata.h"
#include "hashmap.h"
#include "lisp.h"
#include "dbms.h"

int cursors_inuse = 0;

struct TranQuery {
	TranQuery() {
	}
	TranQuery(int t, DbmsQuery* q) : tran(t), query(q) {
	}
	~TranQuery() {
		if (query)
			query->close();
	}
	int tran = 0;
	DbmsQuery* query = nullptr;
};

class DbServerDataImp : public DbServerData {
public:
	DbServerDataImp() : nextQnum(0), nextCnum(0) {
		auth = false;
	}

	void add_tran(int tran) override;
	int add_query(int tran, DbmsQuery* q) override;
	DbmsQuery* get_query(int qn) override;
	bool erase_query(int qn) override;
	Lisp<int> get_trans() override;
	void end_transaction(int tran) override;

	int add_cursor(DbmsQuery* q) override;
	DbmsQuery* get_cursor(int cn) override;
	bool erase_cursor(int cn) override;

	void abort(void (*fn)(int tran)) override;

private:
	int nextQnum;
	int nextCnum;
	HashMap<int, TranQuery> queries;
	HashMap<int, DbmsQuery*> cursors;
	HashMap<int, Lisp<int> > trans; // tran# => list of query#
};

void DbServerDataImp::add_tran(int tran) {
	trans[tran] = Lisp<int>();
}

int DbServerDataImp::add_query(int tran, DbmsQuery* q) {
	queries[nextQnum] = TranQuery(tran, q);
	trans[tran].push(nextQnum);
	return nextQnum++;
}

DbmsQuery* DbServerDataImp::get_query(int qn) {
	TranQuery* tq = queries.find(qn);
	return tq ? tq->query : 0;
}

bool DbServerDataImp::erase_query(int qn) {
	return queries.erase(qn);
}

Lisp<int> DbServerDataImp::get_trans() {
	Lisp<int> t;
	for (HashMap<int, Lisp<int> >::iterator iter = trans.begin();
		 iter != trans.end(); ++iter)
		t.append(iter->key);
	return t;
}

void DbServerDataImp::end_transaction(int tran) {
	for (Lisp<int> q = trans[tran]; !nil(q); ++q)
		queries.erase(*q); // may fail if query was already closed
	// WARNING: this depends on erase calling destructor to close query
	verify(trans.erase(tran));
}

int DbServerDataImp::add_cursor(DbmsQuery* q) {
	++cursors_inuse;
	cursors[nextCnum] = q;
	return nextCnum++;
}

DbmsQuery* DbServerDataImp::get_cursor(int cn) {
	DbmsQuery** pq = cursors.find(cn);
	return pq ? *pq : 0;
}

bool DbServerDataImp::erase_cursor(int cn) {
	--cursors_inuse;
	return cursors.erase(cn);
}

void DbServerDataImp::abort(void (*fn)(int tran)) {
	for (auto qi = queries.begin(); qi != queries.end(); ++qi)
		qi->val.~TranQuery();
	for (auto ci = cursors.begin(); ci != cursors.end(); ++ci) {
		ci->val->close();
		--cursors_inuse;
	}
	for (auto ti = trans.begin(); ti != trans.end(); ++ti)
		fn(ti->key);
}

DbServerData* DbServerData::create() {
	return new DbServerDataImp;
}

#include "suobject.h"

SuObject& dbserver_connections() {
	static SuObject ob(true); // readonly
	return ob;
}
