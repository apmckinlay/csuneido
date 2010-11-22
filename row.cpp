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

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#endif

#include "row.h"
#include <ctype.h>
#include "symbols.h"

// Header ===========================================================

Header Header::project(const Fields& fields) const
	{
	Lisp<Fields> newhdr;
	for (Lisp<Fields> fs = flds; ! nil(fs); ++fs)
		{
		Fields newflds;
		for (Fields g = *fs; ! nil(g); ++g)
			newflds.push(member(fields, *g) ? *g : gcstring("-"));
		newhdr.push(newflds.reverse());
		}
	Fields newcols;
	for (Fields f = fields; ! nil(f); ++f)
		if (member(cols, *f))
			newcols.push(*f);
	return Header(newhdr.reverse(), newcols.reverse());
	}

Header Header::rename(const Fields& from, const Fields& to) const
	{
	int i;
	Lisp<Fields> newhdr;
	for (Lisp<Fields> f = flds; ! nil(f); ++f)
		{
		Fields g = *f;
		if (nil(intersect(from, g)))
			newhdr.push(g);
		else
			{
			Fields newflds;
			for (; ! nil(g); ++g)
				if (-1 == (i = search(from, *g)))
					newflds.push(*g);
				else
					newflds.push(to[i]);
			newhdr.push(newflds.reverse());
			}
		}
	Fields newcols;
	for (Fields c = cols; ! nil(c); ++c)
		if (-1 == (i = search(from, *c)))
			newcols.push(*c);
		else
			newcols.push(to[i]);
	return Header(newhdr.reverse(), newcols.reverse());
	}

bool inflds(const Lisp<Fields>& flds, const gcstring& field)
	{
	for (Lisp<Fields> f = flds; ! nil(f); ++f)
		if (f->member(field))
			return true;
	return false;
	}

// rules are the columns that are NOT in the fields
Fields Header::rules() const
	{
	Fields rules;
	for (Fields c = cols; ! nil(c); ++c)
		if (! inflds(flds, *c))
			rules.push(*c);
	return rules;
	}

// fields are the columns that ARE in the fields
Fields Header::fields() const
	{
	// NOTE: this includes deleted fields - important for output
	if (size() == 1)
		return flds[0];
	if (size() == 2)
		return flds[1];
	verify(size() % 2 == 0);
	Fields fields;
	Lisp<Fields> fs = flds;
	// WARNING: assumes "real" data is in every other (odd) record
	for (++fs; ! nil(fs); ++fs, ++fs)
		for (Fields f = *fs; ! nil(f); ++f)
			if (*f == "-" || ! fields.member(*f))
				fields.push(*f);
	return fields.reverse();
	}

// schema is all the columns with the rules capitalized
Fields Header::schema() const
	{
	Fields schema = fields();
	for (Fields c = cols; ! nil(c); ++c)
		if (! inflds(flds, *c))
			{
			gcstring str(c->str()); // copy
			char* s = str.str();
			*s = toupper(*s);
			schema.append(str);
			}
	return schema;
	}

Lisp<int> Header::output_fldsyms() const
	{
	if (nil(fldsyms))
		{
		// WARNING: this depends on flds[1] being the actual fields
		for (Fields f = fields(); ! nil(f); ++f)
			fldsyms.push(*f == "-" ? -1 : symnum(f->str()));
		fldsyms.reverse();
		}
	return fldsyms;
	}

int Header::timestamp_field() const
	{
	if (! timestamp)
		{
		timestamp = -1; // no timestamp
		for (Fields f = fields(); ! nil(f) && timestamp == -1; ++f)
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

Value Row::getval(const Header& hdr, const gcstring& col) const
	{
	Which w = find(hdr, col);
	if (w.offset >= 0 || ! member(hdr.cols, col))
		return ::unpack(getraw(w));
	// else rule
	if (! surec)
		get_surec(hdr);
	return surec->getdata(col);
	}

gcstring Row::getrawval(const Header& hdr, const gcstring& col) const
	{
	Which w = find(hdr, col);
	if (w.offset >= 0)
		return getraw(w);
	// else rule
	if (! surec)
		get_surec(hdr);
	Value val = surec->getdata(col);
	gcstring s(val.packsize());
	val.pack(s.buf());
	return s;
	}

gcstring Row::getstr(const Header& hdr, const gcstring& col) const
	{
	// TODO: shortcut creating SuString just to extract gcstring
	Value x = getval(hdr, col);
	return x ? x.str() : gcstring();
	}

gcstring Row::getraw(const Header& hdr, const gcstring& col) const
	{
	return getraw(find(hdr, col));
	}

gcstring Row::getraw(const Which& w) const
	{
	return w.offset < 0 ? gcstring("") : (*w.records).getraw(w.offset);
	}

Row::Which Row::find(const Header& hdr, const gcstring& col) const
	{
	int i = -1;
	Records d = data;
	for (Lisp<Fields> f = hdr.flds; ! nil(f); ++f, ++d)
		if (! nil(d) && ! nil(*d) && -1 != (i = search(*f, col)))
			break ;
	return Which(d, i);
	}

bool equal(const Header& hdr, const Row& r1, const Row& r2)
	{
	for (Fields f = hdr.columns(); ! nil(f); ++f)
		if (r1.getraw(hdr, *f) != r2.getraw(hdr, *f))
			return false;
	return true;
	}

SuRecord* Row::get_surec(const Header& hdr) const
	{
	if (! surec)
		surec = new SuRecord(*this, hdr, tran);
	return surec;
	}

void Row::to_heap()
	{
	data = data.copy();
	for (Records d = data; ! nil(d); ++d)
		*d = d->to_heap();
	}

#include "testing.h"

class test_row : public Tests
	{
	TEST(1, create)
		{
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
		std::pair<gcstring,gcstring> p;
		p = *iter; verify(p.first == "sin" && str(p.second) == "123-456-789"); ++iter;
		p = *iter; verify(p.first == "sex" && str(p.second) == "male"); ++iter;
		p = *iter; verify(p.first == "name" && str(p.second) == "fred"); ++iter;
		p = *iter; verify(p.first == "age" && str(p.second) == "42"); ++iter;
		p = *iter; verify(p.first == "phone" && str(p.second) == "249-5050"); ++iter;
		verify(iter == row.end());
		}
	SuString str(const gcstring& packed)
		{ return SuString(::unpack(packed).gcstr()); }

	TEST(2, equal)
		{
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
		verify(! equal(hdr, y, x));
		verify(! equal(hdr, x, y));
		}
	Header mkhdr(char* a, char* b, char* c)
		{
		Fields f;
		f.append(a);
		f.append(b);
		f.append(c);
		Lisp<Fields> h;
		h.push(f);
		return Header(h, f);
		}
	TEST(3, to_heap)
		{
		Records d;
		Record r1;
		r1.addval("one");
		r1.addval("two");
		d.push(r1);
		Record r2;
		r2.addval("three");
		r2.addval("four");
		d.push(r2);
		Row y(d);
		Row z(y);
		z.to_heap();
		asserteq(y, z);
		}
	TEST(4, timestamp)
		{
		verify(mkhdr("a", "b", "c").timestamp_field() == -1);
		verify(mkhdr("a", "b_TS", "c_TS").timestamp_field() == symnum("b_TS"));
		}
	};
REGISTER(test_row);
