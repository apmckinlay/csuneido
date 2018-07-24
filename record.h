#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"
#include "std.h"
#include "mmoffset.h"

class Value;
class Mmfile;

template <class T>
struct RecRep;
struct DbRep;

class Record {
public:
	Record() : crep(nullptr) { // NOLINT
	}
	explicit Record(size_t sz);
	explicit Record(const void* r);
	Record(Mmfile* mmf, Mmoffset offset);
	Record(size_t sz, const void* buf);
	int size() const;
	void addraw(const gcstring& s);
	void addval(Value x);
	void addval(const gcstring& s);
	void addval(const char* s);
	void addval(int x);
	void addnil();
	void addmax();
	void addmmoffset(Mmoffset o) {
		addval(mmoffset_to_int(o));
	}
	gcstring getraw(int i) const;
	Value getval(int i) const;
	gcstring getstr(int i) const;
	int getint(int i) const;
	Mmoffset getmmoffset(int i) const {
		return int_to_mmoffset(getint(i));
	}

	bool hasprefix(const Record& r);
	bool prefixgt(const Record& r);

	void reuse(int n);
	char* alloc(size_t n);

	size_t cursize() const;
	size_t bufsize() const;
	void* copyto(void* buf) const; // buf MUST be cursize()
	void moveto(Mmfile* mmf, Mmoffset offset);
	Record dup() const;
	void truncate(int i);
	void* ptr() const;
	Mmoffset off() const;
	friend bool nil(const Record& r);
	bool operator==(const Record& r) const;
	bool operator<(const Record& r) const;

	// used by tempindex and slots
	int to_int() const;
	static Record from_int(int n, Mmfile* mmf);
	Mmoffset to_int64() const;
	static Record from_int64(Mmoffset n, Mmfile* mmf);

	Record to_heap() const;
	static const Record empty;

private:
	void init(size_t sz);
	int avail() const;
	void grow(int need);
	static void copyrec(const Record& src, Record& dst);
	union {
		RecRep<uint8_t>* crep;
		RecRep<uint16_t>* srep;
		RecRep<uint32_t>* lrep;
		DbRep* dbrep;
	};
};

inline bool nil(const Record& r) {
	return r.crep == 0;
}

Ostream& operator<<(Ostream& os, const Record& r);
