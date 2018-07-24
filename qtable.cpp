// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qtable.h"
#include "qselect.h"
#include "database.h"
#include "thedb.h"
#include "fibers.h" // for yield
#include "qhistable.h"
#include "trace.h"

#define LOG(stuff) TRACE(TABLE, stuff)

Query* Query::make_table(const char* s) {
	return new Table(s);
}

Table::Table(const char* s) : table(s), tbl(theDB()->get_table(table)) {
	if (!theDB()->istable(table))
		except("nonexistent table: " << table);
	singleton = nil(*Table::indexes());
}

void Table::out(Ostream& os) const {
	os << table;
	if (!nil(idxflds))
		os << "^" << idxflds;
}

Fields Table::columns() {
	return theDB()->get_columns(table);
}

Indexes Table::indexes() {
	return theDB()->get_indexes(table);
}

Indexes Table::keys() {
	return theDB()->get_keys(table);
}

double Table::nrecords() {
	return theDB()->nrecords(table);
}

int Table::recordsize() {
	int nrecs = theDB()->nrecords(table);
	return nrecs ? theDB()->totalsize(table) / nrecs : 0;
}

int Table::columnsize() {
	return recordsize() / size(columns());
}

gcstring fields_to_commas(Fields list) {
	gcstring s;
	for (; !nil(list); ++list) {
		s += *list;
		if (!nil(cdr(list)))
			s += ",";
	}
	return s;
}

int Table::keysize(const Fields& index) {
	int nrecs = theDB()->nrecords(table);
	if (nrecs == 0)
		return 0;
	Index* ix = theDB()->get_index(table, fields_to_commas(index));
	verify(ix);
	int nnodes = ix->get_nnodes();
	int nodesize = NODESIZE / (nnodes <= 1 ? 4 : 2);
	return (nnodes * nodesize) / nrecs;
}

int Table::indexsize(const Fields& index) {
	Index* ix = theDB()->get_index(table, fields_to_commas(index));
	verify(ix);
	return ix->get_nnodes() * NODESIZE + index.size();
}

int Table::totalsize() {
	return theDB()->totalsize(table);
}

float Table::iselsize(const Fields& index, const Iselects& iselects) {
	Iselects isels;

	LOG("iselsize " << index << " ? " << iselects);

	// first check for matching a known number of records
	if (member(keys(), index) && size(index) == size(iselects)) {
		int nexact = 1;
		for (isels = iselects; !nil(isels); ++isels) {
			Iselect isel = *isels;
			verify(!isel.none());
			if (isel.type == ISEL_VALUES)
				nexact *= size(isel.values);
			else if (isel.org != isel.end) { // range
				nexact = 0;
				break;
			}
		}
		if (nexact > 0) {
			int nrecs = theDB()->nrecords(table);
			return nrecs ? (float) nexact / nrecs : 0;
			// NOTE: assumes they all exist ???
		}
	}

	// TODO: convert this to use Select::selects()

	for (isels = iselects; !nil(isels); ++isels) {
		Iselect& isel = *isels;
		verify(!isel.none());
		if (isel.one()) {
		} else if (isel.type == ISEL_RANGE) {
			break;
		} else { // set - recurse through values
			Lisp<gcstring> save = isel.values;
			float sum = 0;
			for (Lisp<gcstring> values = isel.values; !nil(values); ++values) {
				isel.values = Lisp<gcstring>(*values);
				sum += iselsize(index, iselects);
			}
			isel.values = save;
			LOG("sum is " << sum);
			return sum;
		}
	}

	// now build the key
	int i = 0;
	Record org;
	Record end;
	for (isels = iselects; !nil(isels); ++isels, ++i) {
		Iselect& isel = *isels;
		verify(!isel.none());
		if (isel.one()) {
			if (isel.type == ISEL_RANGE) {
				org.addraw(isel.org.x);
				end.addraw(isel.org.x);
			} else { // in set
				org.addraw(*isel.values);
				end.addraw(*isel.values);
			}
			if (nil(cdr(isels))) {
				// final exact value (inclusive end)
				++i;
				for (int j = i; j < size(index); ++j)
					end.addmax();
				if (i >= size(index)) // ensure at least one added
					end.addmax();
			}
		} else if (isel.type == ISEL_RANGE) {
			// final range
			org.addraw(isel.org.x);
			end.addraw(isel.end.x);
			++i;
			if (isel.org.d != 0) { // exclusive
				for (int j = i; j < size(index); ++j)
					org.addmax();
				if (i >= size(index)) // ensure at least one added
					org.addmax();
			}
			if (isel.end.d == 0) { // inclusive
				for (int j = i; j < size(index); ++j)
					end.addmax();
				if (i >= size(index)) // ensure at least one added
					end.addmax();
			}
			break;
		} else
			unreachable();
	}
	LOG("=\t" << org << " ->\t" << end);
	float frac = theDB()->rangefrac(table, fields_to_commas(index), org, end);
	LOG("frac " << frac);
	return frac;
}

struct IdxSize {
	IdxSize(Fields index_, int size_) : index(index_), size(size_) {
	}
	Fields index;
	int size;
};

static Lisp<IdxSize*> get_idxs(Table* tbl, Indexes indexes) {
	Lisp<IdxSize*> idxs;
	for (; !nil(indexes); ++indexes)
		idxs.push(new IdxSize(*indexes, tbl->indexsize(*indexes)));
	return idxs.reverse();
}

// find the shortest of indexes with index as a prefix & containing needs
static IdxSize* match(
	Lisp<IdxSize*> idxs, const Fields& index, const Fields& needs) {
	IdxSize* best = NULL;
	for (; !nil(idxs); ++idxs)
		if (prefix((*idxs)->index, index) && subset((*idxs)->index, needs))
			if (best == NULL || best->size > (*idxs)->size)
				best = *idxs;
	return best;
}

int tcn = 0;

const Fields none;

double Table::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze) {
	if (!subset(columns(), needs))
		except("Table::optimize columns does not contain: " << difference(
				   needs, columns()));
	if (!subset(columns(), index))
		return IMPOSSIBLE;
	++tcn;
	Indexes indexes = this->indexes();
	if (nil(indexes))
		return IMPOSSIBLE;
	if (singleton) {
		idxflds = nil(index) ? *indexes : index;
		return recordsize();
	}

	Lisp<IdxSize*> idxs = get_idxs(this, indexes);
	double cost1 = IMPOSSIBLE;
	double cost2 = IMPOSSIBLE;
	double cost3 = IMPOSSIBLE;
	Fields allneeds = set_union(needs, firstneeds);
	IdxSize *idx1, *idx2 = nullptr, *idx3 = nullptr;
	if (nullptr != (idx1 = match(idxs, index, allneeds)))
		// index found that meets all needs
		cost1 = idx1->size; // cost of reading index
	if (!nil(firstneeds) && nullptr != (idx2 = match(idxs, index, firstneeds)))
		// index found that meets firstneeds
		// assume this means we only have to read 75% of data
		cost2 = .75 * totalsize() + // cost of reading data
			idx2->size;             // cost of reading index
	// if nil(allneeds) then this is unnecessary - same as idx1 case
	if (!nil(allneeds) && nullptr != (idx3 = match(idxs, index, none)))
		cost3 = totalsize() + // cost of reading data
			idx3->size;       // cost of reading index
	TRACE(TABLE,
		"optimize " << table << " index " << index
					<< (is_cursor ? " is_cursor" : "")
					<< (freeze ? " FREEZE" : "") << "\n\tneeds: " << needs
					<< "\n\tfirstneeds: " << firstneeds);
	TRACE(TABLE, "\tidx1 " << idx1 << " cost1 " << cost1);
	TRACE(TABLE, "\tidx2 " << idx2 << " cost2 " << cost2);
	TRACE(TABLE, "\tidx3 " << idx3 << " cost3 " << cost3);

	double cost;
	if (cost1 <= cost2 && cost1 <= cost3) {
		cost = cost1;
		if (freeze)
			idxflds = idx1 ? idx1->index : none;
	} else if (cost2 <= cost1 && cost2 <= cost3) {
		cost = cost2;
		if (freeze)
			idxflds = idx2->index;
	} else {
		cost = cost3;
		if (freeze)
			idxflds = idx3->index;
	}
	TRACE(TABLE, "\tchose: idx " << idxflds << " cost " << cost);
	return cost;
}

// used by Select::optimize
void Table::select_index(const Fields& index) {
	idxflds = index;
}

Header Table::header() {
	return Header(
		lisp(singleton ? Fields() : idxflds, theDB()->get_fields(table)),
		theDB()->get_columns(table));
}

void Table::set_index(const Fields& index) {
	idxflds = index;
	idx = (nil(idxflds) || singleton
			? theDB()->first_index(table)
			: theDB()->get_index(table, fields_to_commas(idxflds)));
	verify(idx);
	rewound = true;
}

void Table::select(const Fields& index, const Record& from, const Record& to) {
	if (!prefix(idxflds, index))
		except_err(this << " invalid select: " << index << " " << from << " to "
						<< to);
	sel.org = from;
	sel.end = to;
	rewind();
}

void Table::rewind() {
	rewound = true;
}

void Table::iterate_setup(Dir dir) {
	hdr = header();
	idx = (nil(idxflds) || singleton
			? theDB()->first_index(table)
			: theDB()->get_index(table, fields_to_commas(idxflds)));
	verify(idx);
}

// TODO: factor out code common to get's in Table, TempIndex1, TempIndexN
Row Table::get(Dir dir) {
	Fibers::yieldif();
	verify(tran != INT_MAX);
	if (first) {
		first = false;
		iterate_setup(dir);
	}
	if (rewound) {
		rewound = false;
		iter = singleton ? idx->iter(tran) : idx->iter(tran, sel.org, sel.end);
	}
	if (dir == NEXT)
		++iter;
	else // dir == PREV
		--iter;
	if (iter.eof()) {
		rewound = true;
		return Eof;
	}
	Record r(iter.data());
	if (tbl->num > TN_VIEWS && r.size() > tbl->nextfield)
		except("get: record has more fields (" << r.size() << ") than " << table
											   << " should (" << tbl->nextfield
											   << ")");
	Row row = Row(lisp(iter->key, iter.data()));
	if (singleton) {
		Record key = row_to_key(hdr, row, idxflds);
		if (key < sel.org || sel.end < key) {
			rewound = true;
			return Eof;
		}
	}
	return row;
}

bool Table::output(const Record& r) {
	verify(tran != INT_MAX);
	Record r2(r);
	if (tbl->num > TN_VIEWS && r.size() > tbl->nextfield) {
		r2 = r.dup();
		r2.truncate(tbl->nextfield);
	}
	theDB()->add_record(tran, table, r2);
	return true;
}

#include "testing.h"
#include "tempdb.h"

static void adm(int tran, const char* s) {
	database_admin(s);
}

static void req(int tran, const char* s) {
	except_if(!database_request(tran, s), "FAILED: " << s);
}

TEST(qtable) {
	TempDB tempdb;

	int tran = theDB()->transaction(READWRITE);
	// so trigger searches won't give error
	adm(tran, "create stdlib (group, name, text) key(name)");

	adm(tran,
		"create lines (hdrnum, linenum, desc, amount) key(linenum) "
		"index(hdrnum)");
	req(tran,
		"insert{hdrnum: 1, linenum: 1, desc: \"now\", amount: 10} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 2, desc: \"is\", amount: 20} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 3, desc: \"the\", amount: 30} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 4, desc: \"time\", amount: 40} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 5, desc: \"for\", amount: 50} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 6, desc: \"all\", amount: 60} into lines");
	req(tran,
		"insert{hdrnum: 1, linenum: 7, desc: \"good\", amount: 70} into lines");
	req(tran,
		"insert{hdrnum: 2, linenum: 8, desc: \"men\", amount: 80} into lines");
	req(tran,
		"insert{hdrnum: 2, linenum: 9, desc: \"to\", amount: 90} into lines");
	req(tran,
		"insert{hdrnum: 3, linenum: 10, desc: \"come\", amount: 100} into "
		"lines");

	Table table("lines");
	SuValue* one = new SuNumber(1);
	gcstring onepacked = one->pack();
	Iselect oneisel;
	oneisel.org.x = oneisel.end.x = onepacked;
	float f = table.iselsize(lisp(gcstring("linenum")), lisp(oneisel));
	verify(0 < f && f < .2); // should be .1
	f = table.iselsize(lisp(gcstring("hdrnum")), lisp(oneisel));
	verify(.6 < f && f < .8); // should be .7
}
