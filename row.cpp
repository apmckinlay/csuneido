// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "row.h"
#include "symbols.h"
#include <cctype>
#include "ostreamcon.h"

// Header ===========================================================

Header Header::project(const Fields& fields) const {
	Lisp<Fields> newhdr;
	for (Lisp<Fields> fs = flds; !nil(fs); ++fs) {
		Fields newflds;
		for (Fields g = *fs; !nil(g); ++g)
			newflds.push(member(fields, *g) ? *g : gcstring("-"));
		newhdr.push(newflds.reverse());
	}
	Fields newcols;
	for (Fields f = fields; !nil(f); ++f)
		if (member(cols, *f))
			newcols.push(*f);
	return Header(newhdr.reverse(), newcols.reverse());
}

Header Header::rename(const Fields& from, const Fields& to) const {
	int i;
	Lisp<Fields> newhdr;
	for (Lisp<Fields> f = flds; !nil(f); ++f) {
		Fields g = *f;
		if (nil(intersect(from, g)))
			newhdr.push(g);
		else {
			Fields newflds;
			for (; !nil(g); ++g)
				if (-1 == (i = search(from, *g)))
					newflds.push(*g);
				else
					newflds.push(to[i]);
			newhdr.push(newflds.reverse());
		}
	}
	Fields newcols;
	for (Fields c = cols; !nil(c); ++c)
		if (-1 == (i = search(from, *c)))
			newcols.push(*c);
		else
			newcols.push(to[i]);
	return Header(newhdr.reverse(), newcols.reverse());
}

// returns the cols that are NOT in the fields
// used by query.RuleColumns
Fields Header::rules() const {
	return difference(cols, fields());
}

// WARNING: may return actual data, do not mutate!
Fields Header::fields() const {
	// NOTE: this includes deleted fields - important for output
	if (size() == 1)
		return flds[0];
	if (size() == 2)
		return flds[1];
	verify(size() % 2 == 0);
	Fields fields;
	Lisp<Fields> fs = flds;
	// WARNING: assumes "real" data is in every other (odd) record
	for (++fs; !nil(fs); ++fs, ++fs)
		for (Fields f = *fs; !nil(f); ++f)
			if (*f == "-" || !fields.member(*f))
				fields.push(*f);
	return fields.reverse();
}

bool isSpecialField(const gcstring& col);

// returns the fields plus the rules (capitalized) and _lower!
// used by dbserver to send header to client
Fields Header::schema() const {
	// need copy since append mutates and fields() may return actual data
	Fields schema = fields().copy();
	for (Fields c = cols; !nil(c); ++c)
		if (!member(schema, *c))
			schema.append(isSpecialField(*c) ? *c : c->capitalize());
	return schema;
}

Lisp<int> Header::output_fldsyms() const {
	if (nil(fldsyms)) {
		// WARNING: this depends on flds[1] being the actual fields
		for (Fields f = fields(); !nil(f); ++f)
			fldsyms.push(*f == "-" ? -1 : symnum(f->str()));
		fldsyms.reverse();
	}
	return fldsyms;
}

int Header::timestamp_field() const {
	if (!timestamp) {
		timestamp = -1; // no timestamp
		for (Fields f = fields(); !nil(f) && timestamp == -1; ++f)
			if (has_suffix(f->str(), "_TS"))
				timestamp = symnum(f->str());
	}
	return timestamp;
}

// Row --------------------------------------------------------------

#include "sustring.h"
#include "interp.h"
#include "surecord.h"
#include "pack.h"

Row Row::Eof;

Value Row::getval(const Header& hdr, const gcstring& col) const {
	Which w = find(hdr, col);
	if (w.offset >= 0 || !member(hdr.cols, col))
		return ::unpack(getraw(w));
	// else rule
	if (!surec)
		(void) get_surec(hdr);
	return surec->getdata(symbol(col));
}

gcstring Row::getrawval(const Header& hdr, const gcstring& col) const {
	Which w = find(hdr, col);
	if (w.offset >= 0)
		return getraw(w);
	// else rule
	if (!surec)
		(void) get_surec(hdr);
	Value val = surec->getdata(symbol(col));
	return val.pack();
}

gcstring Row::getstr(const Header& hdr, const gcstring& col) const {
	// TODO: shortcut creating SuString just to extract gcstring
	Value x = getval(hdr, col);
	return x ? x.str() : gcstring();
}

gcstring Row::getraw(const Header& hdr, const gcstring& col) const {
	return getraw(find(hdr, col));
}

gcstring Row::getraw(const Which& w) const {
	return w.offset < 0 ? gcstring("") : (*w.records).getraw(w.offset);
}

Row::Which Row::find(const Header& hdr, const gcstring& col) const {
	if (col == "-")
		return Which(data, -1);
	int i = -1;
	Records d = data;
	for (Lisp<Fields> f = hdr.flds; !nil(f); ++f, ++d)
		if (!nil(d) && !nil(*d) && -1 != (i = search(*f, col)))
			break;
	return Which(d, i);
}

bool equal(const Header& hdr, const Row& r1, const Row& r2) {
	for (Fields f = hdr.columns(); !nil(f); ++f)
		if (r1.getraw(hdr, *f) != r2.getraw(hdr, *f))
			return false;
	return true;
}

SuRecord* Row::get_surec(const Header& hdr) const {
	if (!surec)
		surec = new SuRecord(*this, hdr, tran);
	return surec;
}

//--------------------------------------------------------------------------------

static int deletedSize(const Row& row, const Header& hdr) {
	int deleted = 0;
	Lisp<Record> data = row.data;
	Lisp<Fields> flds = hdr.flds;
	for (; !nil(data); ++data, ++flds) {
		int i = 0;
		for (Fields f = *flds; !nil(f); ++f, ++i)
			if (*f == "-")
				deleted += data->getraw(i).size();
	}
	return deleted;
}

const int SMALL_RECORD = 1024;
const int HUGE_RECORD = 256 * 1024;

static bool shouldRebuild(const Row& row, const Header& hdr, Record rec) {
	if (row.data.size() > 2)
		return true; // must rebuild
	if (rec.cursize() < SMALL_RECORD)
		return false;
	return deletedSize(row, hdr) > rec.cursize() / 3;
}

static bool shouldCompact(Record rec) {
	if (rec.cursize() < SMALL_RECORD)
		return false;
	if (rec.cursize() > HUGE_RECORD)
		return false;
	return (rec.bufsize() - rec.cursize()) > rec.cursize() / 3;
}

Record Row::to_record(const Header& hdr) const {
	Record rec;
	if (data.size() == 1)
		rec = data[0];
	else if (data.size() == 2)
		rec = data[1];
	if (shouldRebuild(*this, hdr, rec)) {
		rec = Record(1000);
		for (Fields f = hdr.fields(); !nil(f); ++f)
			rec.addraw(getraw(hdr, *f));

		// strip trailing empty fields
		int n = rec.size();
		while (rec.getraw(n - 1).size() == 0)
			--n;
		rec.truncate(n);
	}
	if (shouldCompact(rec))
		rec = rec.dup();
	return rec;
}

//--------------------------------------------------------------------------------

#include "testing.h"

static SuString str(const gcstring& packed) {
	return SuString(::unpack(packed).gcstr());
}
TEST(row_create) {
	Lisp<Fields> hdr;

	Fields r;
	r.append("sin");
	r.append("sex");
	r.append("name");
	r.append("age");
	r.append("phone");
	hdr.push(r);
	hdr.push(r); // twice like union

	Header header(hdr, Fields());

	Record s;
	s.addval("123-456-789");
	s.addval("male");
	s.addval("fred");
	s.addval("42");
	s.addval("249-5050");
	Records data;
	data.push(s);
	data.push(Record((void*) 0)); // twice like union

	Row row(data);

	verify(row.getstr(header, "name") == "fred");
	verify(row.getstr(header, "age") == "42");
	verify(row.getstr(header, "phone") == "249-5050");
	verify(row.getstr(header, "sin") == "123-456-789");
	verify(row.getstr(header, "sex") == "male");

	Row::iterator iter = row.begin(header);
	std::pair<gcstring, gcstring> p;
	p = *iter;
	verify(p.first == "sin" && str(p.second) == "123-456-789");
	++iter;
	p = *iter;
	verify(p.first == "sex" && str(p.second) == "male");
	++iter;
	p = *iter;
	verify(p.first == "name" && str(p.second) == "fred");
	++iter;
	p = *iter;
	verify(p.first == "age" && str(p.second) == "42");
	++iter;
	p = *iter;
	verify(p.first == "phone" && str(p.second) == "249-5050");
	++iter;
	verify(iter == row.end());
}

static Header mkhdr(const char* a, const char* b, const char* c) {
	Fields f;
	f.append(a);
	f.append(b);
	f.append(c);
	Lisp<Fields> h;
	h.push(f);
	return Header(h, f);
}
TEST(row_equal) {
	Header hdr(mkhdr("a", "b", "c"));

	Row x; // empty

	Record r;
	r.addval("one");
	r.addval("two");
	Records d;
	d.push(r);
	Row y(d);

	verify(equal(hdr, x, x));
	verify(equal(hdr, y, y));
	verify(!equal(hdr, y, x));
	verify(!equal(hdr, x, y));
}

TEST(row_timestamp) {
	verify(mkhdr("a", "b", "c").timestamp_field() == -1);
	verify(mkhdr("a", "b_TS", "c_TS").timestamp_field() == symnum("b_TS"));
}
