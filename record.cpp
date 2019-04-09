// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "record.h"
#include "sustring.h"
#include "pack.h"
#include "gc.h"
#include "mmfile.h"
#include <climits>
#include <algorithm>
using std::max;
using std::min;

#pragma warning(disable : 4200) // zero-sized array in struct

// ReSharper disable CppSomeObjectMembersMightNotBeInitialized

typedef unsigned char uchar;

enum { MODE0, MODE1, MODE2, MODE4, DBMODE = 0x3f };

const int HDRLEN = 2;

struct RecRep {
	uchar buf[0];

	char mode() const {
		return buf[0] == DBMODE ? DBMODE : buf[0] >> 6;
	}
	int count() const {
		return buf[0] == 0 ? 0 : ((int) (buf[0] & 0x3f) << 8) | buf[1];
	}
	void setHdr(char mode, int count) {
		buf[0] = (mode << 6) | (count >> 8);
		buf[1] = count;
		verify(this->mode() == mode);
		verify(this->count() == count);
	}
	int getOffset(int i) const {
		int j;
		switch (mode()) {
		case MODE1:
			j = HDRLEN + 1 + i;
			return int(buf[j]);
		case MODE2:
			j = HDRLEN + 2 + 2 * i;
			return (int(buf[j]) << 8) | int(buf[j + 1]);
		case MODE4:
			j = HDRLEN + 4 + 4 * i;
			return (int(buf[j]) << 24) | (int(buf[j + 1]) << 16) |
				(int(buf[j + 2]) << 8) | int(buf[j + 3]);
		default:
			unreachable();
		}
	}
	void setOffset(int i, int offset) {
		int j;
		switch (mode()) {
		case MODE1:
			j = HDRLEN + 1 + i;
			buf[j] = offset;
			break;
		case MODE2:
			j = HDRLEN + 2 + 2 * i;
			buf[j] = offset >> 8;
			buf[j + 1] = offset;
			break;
		case MODE4:
			j = HDRLEN + 4 + 4 * i;
			buf[j] = offset >> 24;
			buf[j + 1] = offset >> 16;
			buf[j + 2] = offset >> 8;
			buf[j + 3] = offset;
			break;
		default:
			unreachable();
		}
	}
	gcstring get(int i) const { // NOTE: no alloc - gcstring points to buf
		if (i >= count())
			return gcstring();
		// could use getOffset but inline for performance
		int j, pos, end;
		switch (mode()) {
		case MODE1:
			j = HDRLEN + i;
			end = int(buf[j]);
			pos = int(buf[j + 1]);
			break;
		case MODE2:
			j = HDRLEN + 2 * i;
			end = (int(buf[j]) << 8) | int(buf[j + 1]);
			pos = (int(buf[j + 2]) << 8) | int(buf[j + 3]);
			break;
		case MODE4:
			j = HDRLEN + 4 * i;
			end = (int(buf[j]) << 24) | (int(buf[j + 1]) << 16) |
				(int(buf[j + 2]) << 8) | int(buf[j + 3]);
			pos = (int(buf[j + 4]) << 24) | (int(buf[j + 5]) << 16) |
				(int(buf[j + 6]) << 8) | int(buf[j + 7]);
			break;
		default:
			unreachable();
		}
		return gcstring::noalloc((char*) buf + pos, end - pos);
	}

	int len() const {
		return getOffset(-1);
	}
	void setlen(int len) {
		setOffset(-1, len);
	}

	size_t cursize() const {
		if (buf[0] == 0)
			return 1;
		auto n = count();
		size_t base = 2 + len() - getOffset(n - 1); // data
		size_t size = base + (n + 1);               // offsets
		if (size <= UCHAR_MAX)
			return size;
		size = base + 2 * (n + 1); // offsets
		if (size <= USHRT_MAX)
			return size;
		return base + 4 * (n + 1); // offsets
	}
	int avail() const {
		auto n = count();
		auto offsetSize = 1 << mode();
		return getOffset(n - 1) - (2 * sizeof(short) + (n + 2) * offsetSize);
		// n+2 instead of n+1 to allow for a new item's offset
	}
	char* alloc(size_t len) {
		auto n = count();
		setHdr(mode(), n + 1);
		setOffset(n, getOffset(n - 1) - len);
		return reinterpret_cast<char*>(this) + getOffset(n);
	}
	void add(const gcstring& s) {
		memcpy(alloc(s.size()), s.ptr(), s.size());
	}
	void truncate(int i) {
		if (i < count())
			setHdr(mode(), i);
	}
};

struct DbRep {
	DbRep(Mmfile* mmf, Mmoffset off) : offset(off), rec(mmf->adr(offset)) {
		verify(offset > 0);
	}

	char mode = DBMODE; // first byte to match RecRep rep[0]
	Mmoffset offset;
	Record rec;
};

Record::Record(size_t sz) // NOLINT
	: rep(static_cast<RecRep*>(::operator new(sz + 1, noptrs))) {
	init(sz);
	// ensure that final value is followed by nul (why???)
	reinterpret_cast<char*>(rep)[sz] = 0;
}

Record::Record(size_t sz, const void* buf) // NOLINT
	: rep((RecRep*) buf) {
	init(sz);
}

void Record::init(size_t sz) {
	if (sz <= UCHAR_MAX) {
		rep->setHdr(MODE1, 0);
	} else if (sz <= USHRT_MAX) {
		rep->setHdr(MODE2, 0);
	} else {
		rep->setHdr(MODE4, 0);
	}
	rep->setlen(sz);
}

// used by database
void Record::reuse(int n) {
	auto cr = rep->buf[0] == DBMODE ? dbrep->rec.rep : rep;
	cr->setHdr(cr->mode(), n);
}

Record::Record(const void* r) // NOLINT
	: rep((RecRep*) r) {
}

Record::Record(Mmfile* mmf, Mmoffset offset) // NOLINT
	: dbrep(new DbRep(mmf, offset)) {
}

void* Record::ptr() const {
	if (!rep)
		return nullptr;
	return (void*) (rep->buf[0] == DBMODE ? dbrep->rec.rep : rep);
}

Mmoffset Record::off() const {
	verify(rep->buf[0] == DBMODE);
	verify(dbrep->offset >= 0);
	return dbrep->offset;
}

// the number of elements
int Record::size() const {
	if (!rep)
		return 0;
	auto cr = rep->buf[0] == DBMODE ? dbrep->rec.rep : rep;
	return cr->count();
}

#define CALL(meth) \
	Record r = rep->buf[0] == DBMODE ? dbrep->rec : *this; \
	return r.rep->meth;

gcstring Record::getraw(int i) const {
	if (!rep)
		return gcstring();
	CALL(get(i));
}

Value Record::getval(int i) const {
	return ::unpack(getraw(i));
}

gcstring Record::getstr(int i) const {
	return unpack_gcstr(getraw(i));
}

int Record::getint(int i) const {
	return unpackint(getraw(i));
}

bool Record::hasprefix(Record r) {
	if (r.size() == 0)
		return true;
	for (int i = 0; getraw(i) == r.getraw(i);)
		if (++i >= r.size())
			return true;
	return false;
}

bool Record::prefixgt(Record r) {
	gcstring x, y;
	int n = min(size(), r.size());
	for (int i = 0; i < n; ++i)
		if ((x = getraw(i)) != (y = r.getraw(i)))
			return x > y;
	// prefix equal
	return false;
}

size_t Record::cursize() const {
	if (!rep)
		return 1; // min RecRep size
	CALL(cursize());
}

size_t Record::bufsize() const {
	if (!rep)
		return 1; // min RecRep size
	CALL(len());
}

int Record::avail() const {
	if (!rep)
		return -1; // -1 because you can't even add a 0 length item
	CALL(avail());
}

char* Record::alloc(size_t n) {
	if (avail() < (int) n)
		grow(n);
	CALL(alloc(n));
}

void Record::addnil() {
	alloc(0);
}

void Record::addmax() {
	*alloc(1) = 0x7f;
}

void Record::addraw(const gcstring& s) {
	memcpy(alloc(s.size()), s.ptr(), s.size());
}

void Record::addval(Value x) {
	x.pack(alloc(x.packsize()));
}

void Record::addval(const gcstring& s) {
	SuString str(s);
	addval(&str);
}

void Record::addval(const char* s) {
	addval(gcstring::noalloc(s));
}

void Record::addval(int n) {
	packint(alloc(packsizeint(n)), n);
}

void Record::copyrec(Record src, Record& dst) {
	if (!src.rep)
		return;
	verify(dst.rep);
	verify(dst.rep->buf[0] != DBMODE);
	auto srcrep = (src.rep->buf[0] == DBMODE) ? src.dbrep->rec.rep : src.rep;
	auto n = srcrep->count();
	for (auto i = 0; i < n; ++i)
		dst.rep->add(srcrep->get(i));
	// COULD: copy the data with one memcpy, then adjust offsets
}

void Record::grow(int need) {
	if (!rep) {
		Record dst(max(60, 2 * need));
		rep = dst.rep;
	} else {
		Record dst(2 * (cursize() + need));
		copyrec(*this, dst);
		rep = dst.rep;
	}
}

void* Record::copyto(void* buf) const {
	Record dst(cursize(), buf);
	copyrec(*this, dst);
	return buf;
}

void Record::moveto(Mmfile* mmf, Mmoffset offset) {
	copyto(mmf->adr(offset));
	dbrep = new DbRep(mmf, offset);
}

Record Record::dup() const {
	if (!rep)
		return Record();
	Record dst(cursize());
	copyrec(*this, dst);
	return dst;
}

void Record::truncate(int n) {
	if (!rep)
		return;
	verify(rep->buf[0] != DBMODE);
	rep->truncate(n);
}

// MAYBE: rewrite op== and op< similar to copy

bool Record::operator==(Record r) const {
	if (rep == r.rep || (!rep && r.size() == 0) || (!r.rep && size() == 0))
		return true;
	else if (!rep || !r.rep)
		return false;

	if (size() != r.size())
		return false;
	for (int i = 0; i < size(); ++i)
		if (getraw(i) != r.getraw(i))
			return false;
	return true;
}

bool Record::operator<(Record r) const {
	gcstring x, y;
	int n = min(size(), r.size());
	for (int i = 0; i < n; ++i)
		if ((x = getraw(i)) != (y = r.getraw(i)))
			return x < y;
	// common fields are equal
	return size() < r.size();
}

#include <cctype>

Ostream& operator<<(Ostream& os, Record r) {
	os << '[';
	for (int i = 0; i < r.size(); ++i) {
		if (i > 0)
			os << ',';
		if (r.getraw(i) == "\x7f")
			os << "MAX";
		else
			os << r.getval(i);
	}
	return os << ']';
}

const int DB_BIT = 1 << MM_SHIFT;
const int DB_MASK = (1 << (MM_SHIFT + 1)) - 1;

Mmoffset Record::to_int64() const {
	if (!rep)
		return 0;
	if (rep->buf[0] == DBMODE) {
		verify((dbrep->offset & DB_MASK) == 0);
		return dbrep->offset | DB_BIT;
	} else {
		verify(((int) rep & DB_MASK) == 0);
		return reinterpret_cast<Mmoffset>(rep);
	}
}

Record Record::from_int64(Mmoffset n, Mmfile* mmf) {
	if (n & DB_BIT)
		return Record(mmf, n & ~DB_BIT);
	else
		return Record(reinterpret_cast<void*>(n));
}

inline int mmoffset_to_tagged_int(Mmoffset off) {
	return (off >> MM_SHIFT) | 1;
}

int Record::to_int() const {
	if (!rep)
		return 0;
	if (rep->buf[0] == DBMODE) {
		verify((dbrep->offset & DB_MASK) == 0);
		return mmoffset_to_tagged_int(dbrep->offset);
	} else {
		verify(((int) rep & DB_MASK) == 0);
		return reinterpret_cast<int>(rep);
	}
}

inline Mmoffset tagged_int_to_mmoffset(int n) {
	return static_cast<Mmoffset>(static_cast<unsigned int>(n & ~1)) << MM_SHIFT;
}

Record Record::from_int(int n, Mmfile* mmf) {
	if (n & 1)
		return Record(mmf, tagged_int_to_mmoffset(n));
	else
		return Record(reinterpret_cast<void*>(n));
}

Record const Record::empty;

// test =============================================================

#include "testing.h"

TEST(record_record) {
	Record empty("\x00");
	assert_eq(empty.size(), 0);

	auto dup_empty = Record::empty.dup();
	assert_eq(dup_empty.size(), 0);

	Record r;
	verify(r.size() == 0);

	r.addval("hello");
	verify(r.size() == 1);
	verify(r.getstr(0) == "hello");
	r.addval("world");
	verify(r.size() == 2);
	verify(r.getstr(0) == "hello");
	verify(r.getstr(1) == "world");

	void* buf = new char[32];
	(void) r.copyto(buf);
	Record r2(buf);
	verify(r2 == r);

	verify(r.dup() == r);

	r.addval(1234);
	verify(r.size() == 3);
	verify(r.getstr(0) == "hello");
	verify(r.getstr(1) == "world");
	verify(r.getint(2) == 1234);

	// add enough stuff to use all reps
	auto s = gcstring::noalloc("1234567890123456789012345678901234567890123"
							   "456789012345678901234567890");
	for (int i = 0; i < 1000; ++i) {
		assert_eq(r.size(), i + 3);
		r.addraw(s);
	}
	verify(r.getstr(0) == "hello");
	verify(r.getstr(1) == "world");
	verify(r.getint(2) == 1234);

	Record big;
	big.addnil();
	const int bblen = 100000;
	char* bigbuf = salloc(bblen);
	memset(bigbuf, 0x77, bblen);
	big.addval(SuString::noalloc(bigbuf, bblen));
	int n = big.cursize();
	verify(100000 <= n && n <= 100050);
}

TEST(record_mmoffset) {
	Record r;
	int64_t n = 88888;
	r.addmmoffset(n);
	assert_eq(r.getmmoffset(0), n);
	n = 4 + (static_cast<int64_t>(1) << 32);
	int m = mmoffset_to_int(n);
	assert_eq(n, int_to_mmoffset(m));
	r.addmmoffset(n);
	assert_eq(r.getmmoffset(1), n);
}

TEST(record_to_from_int) {
	Record r;
	r.addval("hello");

	int64_t n64 = r.to_int64();
	Record r2 = Record::from_int64(n64, 0);
	assert_eq(r, r2);

	int n = r.to_int();
	Record r3 = Record::from_int(n, 0);
	assert_eq(r, r3);

	for (int64_t i = 1; i < 16; ++i) {
		Mmoffset mmo = i * 1024 * 1024 * 1024;
		n = mmoffset_to_tagged_int(mmo);
		assert_eq(tagged_int_to_mmoffset(n), mmo);
	}
}
