#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "query.h"

class Query1 : public Query
	{
public:
	explicit Query1(Query* s) : source(s)
		{ }

	bool updateable() const override
		{ return source->updateable(); }
	Query* transform() override
	// also defined by Query2
		{
		source = source->transform();
		return this;
		}
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override
		{ return source->optimize(index, needs, firstneeds, is_cursor, freeze); }
	Query* addindex() override
		{
		source = source->addindex();
		return Query::addindex();
		}
	void set_transaction(int tran) override
		{ source->set_transaction(tran); }

	// estimated result sizes
	double nrecords() override
		{ return source->nrecords(); }
	int recordsize() override
		{ return source->recordsize(); }
	int columnsize() override
		{ return source->columnsize(); }

	Lisp<Fixed> fixed() const override
		{ return source->fixed(); }
	void best_prefixed(Indexes indexes, const Fields& by,
		const Fields& needs, bool is_cursor,
		Fields& best_index, double& best_cost);

	void close(Query* q) override
		{ source->close(q); }

	Query* source;
	};

bool isfixed(Lisp<Fixed> f, const gcstring& field);

bool prefixed(Fields ix, Fields by, const Lisp<Fixed>& fixed);

class Query2 : public Query1
	{
public:
	Query2(Query* s1, Query* s2) : Query1(s1), source2(s2)
		{ }
	bool updateable() const override
		{ return false; }
	Query* transform() override
		{
		source = source->transform();
		source2 = source2->transform();
		return this;
		}
	Query* addindex() override
		{
		source = source->addindex();
		source2 = source2->addindex();
		return Query::addindex();
		}
	void set_transaction(int tran) override
		{
		source->set_transaction(tran);
		source2->set_transaction(tran);
		}
	Lisp<Fixed> fixed() const override
		{ return Lisp<Fixed>(); }

	void close(Query* q) override
		{
		source->close(q);
		source2->close(q);
		}

	Query* source2;
	};

struct Keyrange
	{
	Keyrange() : org(keymin), end(keymax)
		{}
	Keyrange(const Record& o, const Record& e) : org(o), end(e)
		{ }
	explicit operator bool() const
		{ return ! (end < org); }

	Record org;
	Record end;
	};

inline Ostream& operator<<(Ostream& os, const Keyrange& r)
	{ return os << "{" << r.org << " => " << r.end << "}"; }
