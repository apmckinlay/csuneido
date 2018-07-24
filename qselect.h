#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"
#include "qexpr.h"

struct Point { // only used by Iselect
	gcstring x;
	int d = 0; // 0 if inclusive, +1 or -1 if exclusive
	bool operator<(const Point& q) const {
		return x == q.x ? d < q.d : x < q.x;
	}
	bool operator==(const Point& q) const {
		return x == q.x && d == q.d;
	}
};

// NOTE: the gcstring's in Point & Iselect are packed

extern gcstring fieldmax;

enum IselType { ISEL_RANGE, ISEL_VALUES };

// TODO: handle interchangability of a single value and range where org = end
struct Iselect {
	IselType type;
	// range
	Point org;
	Point end;
	// set
	Lisp<gcstring> values;

	Iselect() : type(ISEL_RANGE) {
		org.x = "";
		end.x = fieldmax;
	}
	bool inrange(const gcstring& x);
	bool matches(const gcstring& value);
	void and_with(const Iselect& r);
	bool all() const {
		return org.x == "" && org.d == 0 && end.x == fieldmax;
	}
	bool none() const {
		return org > end && size(values) == 0;
	}
	bool one() const {
		return org == end || size(values) == 1;
	}
	bool operator==(const Iselect& y) const {
		return org == y.org && end == y.end;
	}
	friend Ostream& operator<<(Ostream& os, const Iselect& r);
};

typedef Lisp<Iselect> Iselects;
