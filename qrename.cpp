// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qrename.h"

Query* Query::make_rename(Query* source, const Fields& f, const Fields& t)
	{
	return new Rename(source, f, t);
	}

Rename::Rename(Query* source, const Fields& f, const Fields& t)
	: Query1(source), from(f), to(t)
	{
	Fields src = source->columns();
	if (! subset(src, from))
		except("rename: nonexistent column(s): " << difference(from, src));
	Fields dups = intersect(src, to);
	if (! nil(dups))
		except("rename: column(s) already exist: " << dups);

	// also rename dependencies (_deps)
	for (Fields fs = from, ts = to; ! nil(fs); ++fs, ++ts)
		{
		gcstring deps(*fs + "_deps");
		if (src.member(deps))
			{
			from.push(deps);
			to.push(*ts + "_deps");
			}
		}
	}

void Rename::out(Ostream& os) const
	{
	os << *source << " RENAME ";
	for (Fields f = from, t = to; ! nil(f); ++f)
		{
		os << *f << " to " << *t;
		if (! nil(++t))
			os << ", ";
		}
	}

Query* Rename::transform()
	{
	// remove empty Renames
	if (nil(from))
		return source->transform();
	// combine Renames
	if (Rename* r = dynamic_cast<Rename*>(source))
		{
		Fields from2, to2;
		for (Fields f = from, t = to; ! nil(f); ++f, ++t)
			if (! member(r->to, *f))
				{
				from2.push(*f);
				to2.push(*t);
				}
		to = concat(rename_fields(r->to, from, to), to2.reverse());
		from = concat(r->from, from2.reverse());
		source = r->source;
		return transform();
		}
	source = source->transform();
	return this;
	}

double Rename::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	// NOTE: optimize1 to bypass tempindex
	return source->optimize1(
		rename_fields(index, to, from),
		rename_fields(needs, to, from),
		rename_fields(firstneeds, to, from),
		is_cursor, freeze);
	}

Fields Rename::rename_fields(Fields f, Fields from, Fields to)
	{
	Fields new_fields;
	for (; ! nil(f); ++f)
		{
		int i = search(from, *f);
		new_fields.push(i == -1 ? *f : to[i]);
		}
	return new_fields.reverse();
	}

Indexes Rename::rename_indexes(Indexes i, Fields from, Fields to)
	{
	Indexes new_idxs;
	for (; ! nil(i); ++i)
		new_idxs.push(rename_fields(*i, from, to));
	return new_idxs.reverse();
	}

Header Rename::header()
	{
	return source->header().rename(from, to);
	}
