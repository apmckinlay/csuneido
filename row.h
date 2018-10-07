#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "lisp.h"
#include "record.h"
#include "gcstring.h"
#include <utility> // for pair
#include <algorithm>
using std::min;

typedef Lisp<gcstring> Fields;
typedef Lisp<Record> Records;

class Header {
public:
	Header() {
	}
	Header(const Lisp<Fields>& h, const Lisp<gcstring>& c) : flds(h), cols(c) {
	}
	int size() const {
		return flds.size();
	}
	Header project(const Fields& fields) const;
	Header rename(const Fields& from, const Fields& to) const;
	Fields columns() const {
		return cols;
	}
	Fields rules() const;
	Fields fields() const;
	Fields schema() const;
	Lisp<int> output_fldsyms() const;
	int timestamp_field() const;

	Lisp<Fields> flds;
	Fields cols;

private:
	mutable Lisp<int> fldsyms;
	mutable int timestamp = 0;
};

inline bool nil(const Header& hdr) {
	return nil(hdr.flds);
}

inline Header operator+(const Header& x, const Header& y) {
	return Header(concat(x.flds, y.flds), set_union(x.cols, y.cols));
}

inline Ostream& operator<<(Ostream& os, const Header& x) {
	os << x.flds;
	return os;
}

class SuRecord;

class Row {
public:
	Row() {
	}
	explicit Row(Record d, Mmoffset ra = 0) : data(d), recadr(ra) {
		verify(ra >= 0);
	}
	explicit Row(const Records& d) : data(d) {
	}
	gcstring getraw(const Header& hdr, const gcstring& colname) const;
	gcstring getrawval(const Header& hdr, const gcstring& col) const;
	Value getval(const Header& hdr, const gcstring& colname) const;
	gcstring getstr(const Header& hdr, const gcstring& colname) const;
	bool operator==(const Row& r) const {
		return data == r.data;
	}
	friend bool equal(const Header& hdr, const Row& r1, const Row& r2);
	friend Row operator+(const Row& r1, const Row& r2);
	friend Ostream& operator<<(Ostream& os, const Row& row);
	Record to_record(const Header& hdr) const;

	class iterator { // used by select
	public:
		iterator() { // end
		}
		iterator(const Lisp<Fields>& f, const Records& d) : flds(f), data(d) {
			if (nil(flds))
				data = Lisp<Record>();
			else if (!nil(data)) {
				fld = -1;
				n = min(flds->size(), (int) data->size()); // vc++ needs (int)
				operator++(); // to advance to first field & value
			}
		}
		std::pair<gcstring, gcstring> operator*() {
			return std::make_pair((*flds)[fld], data->getraw(fld));
		}
		iterator& operator++() {
			++fld;
			while (fld >= n) {
				// advance to next record in row
				++flds;
				++data;
				fld = 0;
				if (nil(data) || nil(flds)) {
					data = Lisp<Record>();
					break;
				}
				n = min(flds->size(), (int) data->size()); // vc++ needs (int)
			}
			return *this;
		}
		iterator operator++(int) {
			iterator ret = *this;
			operator++();
			return ret;
		}
		bool operator==(const iterator& iter) const {
			return data == iter.data && fld == iter.fld;
		}
		Fields fields() const {
			return *flds;
		}

	private:
		Lisp<Fields> flds;
		Lisp<Record> data;
		int fld = 0;
		int n = 0; // number of fields in current data
	};
	iterator begin(const Header& hdr) const {
		return iterator(hdr.flds, data);
	}
	static iterator end() {
		return iterator();
	}
	SuRecord* get_surec(const Header& hdr) const;

	void set_transaction(int t) {
		tran = t;
	}

	Records data;
	Mmoffset recadr =
		0; // if Row contains single update-able record, this is its address
	static Row Eof;

private:
	struct Which {
		Which(const Records& r, short o) : records(r), offset(o) {
		}
		const Records records;
		short offset;
	};
	Which find(const Header& hdr, const gcstring& col) const;
	gcstring getraw(const Which& w) const;

	int tran = -1;
	mutable SuRecord* surec = nullptr;
};

inline Row operator+(const Row& r1, const Row& r2) {
	return Row(concat(r1.data, r2.data));
}

inline Ostream& operator<<(Ostream& os, const Row& row) {
	return os << row.data;
}

Record row_to_key(const Header& hdr, const Row& row, const Fields& flds);
