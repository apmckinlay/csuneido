#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"
#include "lisp.h"
#include "row.h"
#include "value.h"
#include "dir.h"
#include <cfloat>

typedef gcstring Field;
typedef Lisp<Field> Fields;
typedef Lisp<Field> QIndex;
typedef Lisp<QIndex> Indexes;

const int WRITE_FACTOR = 4;  // cost of writing index relative to reading data
const int OUT_OF_ORDER = 10; // minimal penalty for changing order of operations
const double IMPOSSIBLE = DBL_MAX / 10; // allow for adding impossibles together

bool is_admin(const char* s);
bool is_request(const char* s);
void database_admin(const char* s);
int database_request(int tran, const char* s);

extern Record keymin;
extern Record keymax;

class Expr;

class QueryCache {
private:
	struct CacheEntry {
		CacheEntry(const Fields& i, const Fields& n, const Fields& n1, bool ic,
			double c)
			: index(i), needs(n), firstneeds(n1), is_cursor(ic), cost(c) {
		}
		Fields index;
		Fields needs;
		Fields firstneeds;
		bool is_cursor;
		double cost;
	};
	Lisp<CacheEntry> entries;

public:
	void add(const Fields& index, const Fields& needs, const Fields& firstneeds,
		bool is_cursor, double cost) {
		verify(cost >= 0);
		entries.push(CacheEntry(index, needs, firstneeds, is_cursor, cost));
	}
	double get(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor) const {
		for (Lisp<CacheEntry> i = entries; !nil(i); ++i)
			if (i->index == index && i->needs == needs &&
				i->firstneeds == firstneeds && i->is_cursor == is_cursor)
				return i->cost;
		return -1;
	}
};

struct Fixed {
	Fixed(const gcstring& f, Value x) : field(f), values(lisp(x)) {
	}
	Fixed(const gcstring& f, Lisp<Value> vs) : field(f), values(vs) {
	}
	gcstring field;
	Lisp<Value> values;
};
inline bool operator==(const Fixed& f1, const Fixed& f2) {
	return f1.field == f2.field && f1.values == f2.values;
}
inline Ostream& operator<<(Ostream& os, const Fixed& f) {
	return os << f.field << ":" << f.values;
}

class Query { // interface
public:
	// factory methods - used by QueryParser
	static Query* make_sort(Query* source, bool r, const Fields& s);
	static Query* make_rename(Query* source, const Fields& f, const Fields& t);
	static Query* make_extend(Query* source, const Fields& f, Lisp<Expr*> e);
	static Query* make_project(Query* source, const Fields& f, bool allbut);
	static Query* make_summarize(Query* source, const Fields& p,
		const Fields& c, const Fields& f, const Fields& o);
	static Query* make_join(Query* s1, Query* s2, Fields by);
	static Query* make_leftjoin(Query* s1, Query* s2, Fields by);
	static Query* make_product(Query* s1, Query* s2);
	static Query* make_union(Query* s1, Query* s2);
	static Query* make_intersect(Query* s1, Query* s2);
	static Query* make_difference(Query* s1, Query* s2);
	static Query* make_select(Query* s, Expr* e);
	static Query* make_table(const char* s);
	static Query* make_history(const gcstring& table);

	static Expr* make_constant(Value x);
	static Expr* make_identifier(const gcstring& s);
	static Expr* make_unop(short o, Expr* a);
	static Expr* make_call(Expr* ob, const gcstring& id, const Lisp<Expr*>& e);
	static Expr* make_binop(short o, Expr* l, Expr* r);
	static Expr* make_triop(Expr* e, Expr* t, Expr* f);
	static Expr* make_in(Expr* e, const Lisp<Value>& v);
	static Expr* make_or(const Lisp<Expr*>& e);
	static Expr* make_and(const Lisp<Expr*>& e);

	Query();
	virtual ~Query() = default;

	virtual void set_transaction(int tran) = 0;

	// iteration
	static Row Eof;
	virtual Header header() = 0;
	virtual Indexes indexes() = 0;
	virtual Fields ordering() {
		return Fields();
	} // overridden by QSort
	virtual void select(
		const Fields& index, const Record& from, const Record& to) = 0;
	void select(const Fields& index, const Record& key);
	virtual void rewind() = 0;
	virtual Row get(Dir) {
		error("not implemented yet");
	}
	virtual Lisp<Fixed> fixed() const {
		return Lisp<Fixed>();
	}

	// updating
	virtual bool updateable() const {
		return false;
	}
	virtual bool output(const Record&);

	virtual void close(Query*) = 0;

	// protected:
	virtual void out(Ostream&) const = 0;
	virtual Fields columns() = 0;
	virtual Indexes keys() = 0;
	virtual Query* transform() {
		return this;
	}
	double optimize(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze);
	double optimize1(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze);
	virtual double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) = 0;
	Fields key_index(const Fields& needs);
	// estimated result sizes
	virtual double nrecords() = 0;
	virtual int recordsize() = 0;
	virtual int columnsize() = 0;
	QueryCache cache;

	// used to insert TempIndex nodes
	virtual Query* addindex(); // redefined by Query1 and Query2

private:
	Fields tempindex;
};

enum { IS_CURSOR = true };

Query* query(const char* s, bool is_cursor = false);
Query* parse_query(const char* s);
Expr* parse_expr(const char* s);
Query* query_setup(Query* q, bool is_cursor = false);
void trace_tempindex(Query* q);

Ostream& operator<<(Ostream& os, const Query& query);
inline Ostream& operator<<(Ostream& os, Query* query) {
	return os << *query;
}

gcstring fields_to_commas(Fields list);
