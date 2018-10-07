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

// Record is a pointer to a block of memory containing a list of fields
// Since it is just a pointer it should normally be passed by value
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
	void addval(int n);
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

	bool hasprefix(Record r);
	bool prefixgt(Record r);

	void reuse(int n);
	char* alloc(size_t n);

	size_t cursize() const;
	size_t bufsize() const;
	void* copyto(void* buf) const; // buf MUST be cursize()
	void moveto(Mmfile* mmf, Mmoffset offset);
	Record dup() const;
	void truncate(int n);
	void* ptr() const;
	Mmoffset off() const;
	friend bool nil(Record r);
	bool operator==(Record r) const;
	bool operator<(Record r) const;

	// used by tempindex and slots
	int to_int() const;
	static Record from_int(int n, Mmfile* mmf);
	Mmoffset to_int64() const;
	static Record from_int64(Mmoffset n, Mmfile* mmf);

	static const Record empty;

private:
	void init(size_t sz);
	int avail() const;
	void grow(int need);
	static void copyrec(Record src, Record& dst);
	union {
		RecRep<uint8_t>* crep;
		RecRep<uint16_t>* srep;
		RecRep<uint32_t>* lrep;
		DbRep* dbrep;
	};
};

inline bool nil(Record r) {
	return r.crep == nullptr;
}

Ostream& operator<<(Ostream& os, Record r);
