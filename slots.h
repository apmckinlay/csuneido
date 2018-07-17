#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <algorithm>
#include <iterator>
#include "std.h"
#include "lisp.h"
#include "record.h"
#include "except.h"

const int mem = 4000;

inline Record keydup(const Record& key)
	{ return key.dup(); }

inline int keydup(int key)
	{ return key; }

// array of key records, used by Index
class Vslot
	{
public:
	Vslot()
		{ }
	explicit Vslot(const Record& r) : key(r)
		{ }
	explicit Vslot(void* k) : key(k)
		{ }
	typedef Record Key;
	Mmoffset adr()
		{
		verify(key.size() > 0);
		return key.getmmoffset(key.size() - 1);
		}
	void copy(const Vslot& from)
		{ key = from.key.dup(); }
	bool operator<(const Vslot& y) const
		{ return key < y.key; }
	bool operator==(const Vslot& y) const
		{ return key == y.key; }
	bool operator!=(const Vslot& y) const
		{ return key != y.key; }

	Record key;
	};

inline bool nil(const Vslot& vs)
	{ return nil(vs.key); }

class Vslots
	{
public:
	Vslots() : prev(sizeof t), sz(0)  // NOLINT
		{ memset(t, 7, sizeof t); }
	class iterator
		{
		friend class Vslots;
	public:
		using difference_type = int;
		using value_type = Vslot;
		using pointer = const Vslot*;
		using reference = const Vslot&;
		using iterator_category = std::random_access_iterator_tag;

		iterator() = default;

		Vslot operator*() const
			{
			verify(t);
			return Vslot((char*) t + t[i]);
			}
		iterator& operator++()
			{ ++i; return *this; }
		iterator& operator+=(int n)
			{ i += n; return *this; }
		iterator&  operator--()
			{ verify(i > 0); --i; return *this; }
		iterator operator+(int d) const
			{ return iterator(t, i + d); }
		iterator operator-(int d) const
			{ return iterator(t, i - d); }
		int operator-(const iterator& iter) const
			{ return i - iter.i; }
		bool operator==(const iterator& iter) const
			{ return i == iter.i; }
		bool operator!=(const iterator& iter) const
			{ return i != iter.i; }
		bool operator<(const iterator& iter) const
			{ return i < iter.i; }
	private:
		iterator(short* _t, short _i) : t(_t), i(_i)
			{ }
		short* t = nullptr;
		short i = 0;
		};
	bool empty() const
		{ return sz == 0; }
	int capacity() const
		{ return sizeof t / sizeof (short); }
	int size() const
		{ return sz; }
	iterator begin()
		{ return iterator(t,0); }
	iterator end()
		{ return iterator(t,sz); }
	Vslot operator[](int i)
		{ verify(0 <= i && i < sz); return Vslot((char*) t + t[i]); }
	Vslot front()
		{ verify(sz > 0); return Vslot((char*) t + t[0]); }
	Vslot back()
		{ verify(sz > 0); return Vslot((char*) t + t[sz - 1]); }
	void push_back(const Vslot& x)
		{
		int n = x.key.cursize();
		ck(n);
		prev -= n;
		x.key.copyto((char*) t + prev);
		t[sz++] = prev;
		}
	void pop_back()
		{ verify(sz > 0); erase(end() - 1); }
	bool insert(const iterator& position, const Vslot& x)
		{
		// prepend to heap
		int n = x.key.cursize();
		if ((char*) (t + sz + 1) >= (char*) t + prev - n)
			return false;
		prev -= n;
		x.key.copyto((char*) t + prev);
		// insert into vector
		std::copy_backward(t + position.i, t + sz, t + sz + 1);
		t[position.i] = prev;
		++sz;
		return true;
		}
	void insert(const iterator& position, iterator first, const iterator& last)
		{
		int n = last - first;
		int i = position.i;
		std::copy_backward(t + i, t + sz, t + sz + n);
		for (; first != last; ++first)
			{
			Vslot x = *first;
			int len = x.key.cursize();
			ck(len);
			prev -= len;
			x.key.copyto((char*) t + prev);
			t[i++] = prev;
			++sz;
			}
		}
	void append(const iterator& first, const iterator& last)
		{ insert(end(), first, last); }
	void erase(const iterator& position)
		{
		// erase from heap
		int n = (*position).key.cursize();
		int offset = t[position.i];
		memmove((char*) t + prev + n, (char*) t + prev, offset - prev);
		prev += n;
		// erase from vector
		std::copy(t + position.i + 1, t + sz, t + position.i);
		t[--sz] = 0;
		// adjust vector
		for (int i = 0; i < sz; ++i)
			if (t[i] < offset)
				t[i] += n;
		}
	void erase(const iterator& first, const iterator& last)
		{
		// this is not the fastest method, but it's easy!
		for (iterator i = last; i != first; --i)
			erase(i - 1);
		}
	int remaining() const
		{ return ((char*) t + prev) - (char*) (t + sz); }
private:
	short t[(mem - sizeof (short) - sizeof (short)) / sizeof (short)];	// array of offsets
	short prev;		// points to start of heap (grows downward)
	short sz;
	int heapsize() const
		{ return sizeof t - prev; }
	void ck(int n)
		{ verify((char*) (t + sz) < (char*) t + prev - n); }
	};

// array of variable length key and int adr
class VFslot
	{
public:
	typedef Record Key;
	Key key;
	Mmoffset adr = 0; // points to btree node
	VFslot()
		{ }
	explicit VFslot(const Record& k, Mmoffset a = 0) : key(k), adr(a)
		{ }
	explicit VFslot(void* k, Mmoffset a = 0) : key(k), adr(a)
		{ }
	void copy(const VFslot& from)
		{
		adr = from.adr;
		key = from.key.dup();
		}
	bool operator<(const VFslot& y) const
		{ return key < y.key; }
	bool operator==(const VFslot& y) const
		{ return key == y.key; }
	bool operator!=(const VFslot& y) const
		{ return key != y.key; }
	};
class VFslots
	{
public:
	struct slot  // NOLINT
		{
		short offset;
		Mmoffset32 adr;
		};
	VFslots() : prev(sizeof t), sz(0)
		{ memset(t, 7, sizeof t); }
	class iterator
		{
		friend class VFslots;
	public:
		using difference_type = int;
		using value_type = VFslot;
		using pointer = const VFslot*;
		using reference = const VFslot&;
		using iterator_category = std::random_access_iterator_tag;

		iterator()
			{ }
		VFslot operator*() const
			{
			verify(t);
			return VFslot((char*) t + t[i].offset, t[i].adr.unpack());
			}
		iterator& operator++()
			{ ++i; return *this; }
		iterator& operator+=(int n)
			{ i += n; return *this; }
		iterator&  operator--()
			{ verify(i > 0); --i; return *this; }
		iterator operator+(int d) const
			{ return iterator(t, i + d); }
		iterator operator-(int d) const
			{ return iterator(t, i - d); }
		int operator-(const iterator& iter) const
			{ return i - iter.i; }
		bool operator==(const iterator& iter) const
			{ return i == iter.i; }
		bool operator!=(const iterator& iter) const
			{ return i != iter.i; }
		bool operator<(const iterator& iter) const
			{ return i < iter.i; }
		void update(Mmoffset adr)
			{
			verify(t);
			t[i].adr = adr;
			}
	private:
		iterator(slot* _t, short _i) : t(_t), i(_i)
			{ }
		slot* t = nullptr;
		short i = 0;
		};
	bool empty() const
		{ return sz == 0; }
	int capacity() const
		{ return sizeof t / sizeof (slot); }
	int size() const
		{ return sz; }
	iterator begin()
		{ return iterator(t,0); }
	iterator end()
		{ return iterator(t,sz); }
	VFslot operator[](int i)
		{ verify(0 <= i && i < sz); return VFslot((char*) t + t[i].offset, t[i].adr.unpack()); }
	VFslot front()
		{ verify(sz > 0); return VFslot((char*) t + t[0].offset, t[0].adr.unpack()); }
	VFslot back()
		{ verify(sz > 0); return VFslot((char*) t + t[sz - 1].offset, t[sz - 1].adr.unpack()); }
	void push_back(const VFslot& x)
		{
		int n = x.key.cursize();
		ck(n);
		prev -= n;
		x.key.copyto((char*) t + prev);
		t[sz].offset = prev;
		t[sz].adr = x.adr;
		++sz;
		}
	void pop_back()
		{ verify(sz > 0); erase(end() - 1); }
	bool insert(const iterator& position, const VFslot& x)
		{
		// prepend to heap
		int n = x.key.cursize();
		if ((char*) (t + sz + 1) >= (char*) t + prev - n)
			return false;
		prev -= n;
		x.key.copyto((char*) t + prev);
		// insert into vector
		std::copy_backward(t + position.i, t + sz, t + sz + 1);
		t[position.i].offset = prev;
		t[position.i].adr = x.adr;
		++sz;
		return true;
		}
	void insert(const iterator& position, iterator first, const iterator& last)
		{
		int n = last - first;
		int i = position.i;
		std::copy_backward(t + i, t + sz, t + sz + n);
		for (; first != last; ++first)
			{
			VFslot x = *first;
			int len = x.key.cursize();
			ck(len);
			prev -= len;
			x.key.copyto((char*) t + prev);
			t[i].offset = prev;
			t[i].adr = x.adr;
			++sz; ++i;
			}
		}
	void append(const iterator& first, const iterator& last)
		{ insert(end(), first, last); }
	void erase(const iterator& position)
		{
		// erase from heap
		int n = (*position).key.cursize();
		int offset = t[position.i].offset;
		memmove((char*) t + prev + n, (char*) t + prev, offset - prev);
		prev += n;
		// erase from vector
		std::copy(t + position.i + 1, t + sz, t + position.i);
		t[--sz].offset = 0;
		t[sz].adr = 0; // to aid garbage collection
		// adjust vector
		for (int i = 0; i < sz; ++i)
			if (t[i].offset < offset)
				t[i].offset += n;
		}
	void erase(const iterator& first, const iterator& last)
		{
		// this is not the fastest method, but it's easy!
		for (iterator i = last; i != first; --i)
			erase(i - 1);
		}
	int remaining() const
		{ return ((char*) t + prev) - (char*) (t + sz); }
private:
	slot t[(mem - sizeof (short) - sizeof (short)) / sizeof (slot)];	// array of offsets
	short prev;		// points to start of heap (grows downward)
	short sz;
	int heapsize()
		{ return sizeof t - prev; }
	void ck(int n)
		{ verify((char*) (t + sz) < (char*) t + prev - n); }
	};

inline Ostream& operator<<(Ostream& os, VFslots& x)
	{
	for (VFslots::iterator i = x.begin(); i != x.end(); ++i)
		os << (*i).key << "+";
	return os;
	}

struct Vdata
	{
	// TODO: only take the data not the keys i.e. every other record
	enum { maxvdata = 200 };
	explicit Vdata(Lisp<Record> rs)  // NOLINT
		{
		n = 0;
		for (; ! nil(rs); ++rs)
			{
			verify(n < maxvdata);
			r[n++] = rs->to_int();
			}
		}
	size_t len() const
		{ return sizeof (int) + n * sizeof (int); }
	Vdata* dup() const
		{
		int sz = len();
		return (Vdata*) memcpy(new char[sz], this, sz);
		}
	// operator== is used by Project
	bool operator==(const Vdata& d2) const
		{
		if (n != d2.n)
			return false;
		for (int i = 0; i < n; ++i)
			if (r[i] != d2.r[i])
				return false;
		return true;
		}
	int n;
	int r[maxvdata]; // actual size may vary
	};

inline Ostream& operator<<(Ostream& os, const Vdata& vd)
	{
	for (int i = 0; i < vd.n; ++i)
		os << vd.r[i] << " ";
	return os;
	}

// array of variable length key and variable length data
struct VVslot
	{
	typedef Record Key;
	Key key;
	Vdata* data = nullptr;
	VVslot() = default;

	explicit VVslot(const Record& k, Vdata* d = 0) : key(k), data(d)
		{ }
	explicit VVslot(void* k, void* d = 0) : key(k), data((Vdata*) d)
		{ }
	void copy(const VVslot& from)
		{
		key = from.key.dup();
		data = from.data->dup();
		}
	bool operator<(const VVslot& y) const
		{ return key < y.key; }
	bool operator==(const VVslot& y) const
		{ return key == y.key; }
	bool operator!=(const VVslot& y) const
		{ return key != y.key; }
	};

// have to round record sizes to multiples of 4 to ensure address's are alligned
// so that garbage collection sees references (for in-memory temporary indexes)
inline size_t roundup(size_t n)
	{ return ((n - 1) | 3) + 1; }

class VVslots
	{
public:
	// ReSharper disable once CppPossiblyUninitializedMember
	VVslots() : prev(sizeof t), sz(0)  // NOLINT
		{ }
	class iterator
		{
		friend class VVslots;
	public:
		using difference_type = int;
		using value_type = VVslot;
		using pointer = const VVslot*;
		using reference = const VVslot&;
		using iterator_category = std::random_access_iterator_tag;

		iterator() = default;

		VVslot operator*() const
			{
			Record key((char*) t + t[i]);
			char* data = (char*) t + t[i] + roundup(key.cursize());
			return VVslot(key, (Vdata*) data);
			}
		iterator& operator++()
			{ ++i; return *this; }
		iterator& operator+=(int n)
			{ i += n; return *this; }
		iterator& operator--()
			{ --i; return *this; }
		iterator operator+(int d)
			{ return iterator(t, i + d); }
		iterator operator-(int d)
			{ return iterator(t, i - d); }
		int operator-(const iterator& iter) const
			{ return i - iter.i; }
		bool operator==(const iterator& iter) const
			{ return i == iter.i; }
		bool operator!=(const iterator& iter) const
			{ return i != iter.i; }
		bool operator<(const iterator& iter) const
			{ return i < iter.i; }
	private:
		iterator(short* _t, short _i) : t(_t), i(_i)
			{ }
		short* t = nullptr;
		short i = 0;
		};
	bool empty() const
		{ return sz == 0; }
	int capacity() const
		{ return sizeof t / sizeof (short); }
	int size() const
		{ return sz; }
	iterator begin()
		{ return iterator(t,0); }
	iterator end()
		{ return iterator(t,sz); }
	VVslot operator[](int i)
		{ verify(0 <= i && i < sz); iterator iter(t, i); return *iter; }
	VVslot front()
		{ verify(sz > 0); iterator i(begin()); return *i; }
	VVslot back()
		{ verify(sz > 0); iterator i(end()); return *--i; }
	void push_back(const VVslot& x)
		{
		int kn = roundup(x.key.cursize());
		int dn = x.data->len();
		ck(kn + dn);
		prev -= (short) (kn + dn);
		x.key.copyto((char*) t + prev);
		memcpy((char*) t + prev + kn, x.data, dn);
		t[sz++] = prev;
		}
	void pop_back()
		{ verify(sz > 0); iterator i(end()); erase(i - 1); }
	bool insert(const iterator& position, const VVslot& x)
		{
		// prepend to heap
		int kn = roundup(x.key.cursize());
		int dn = x.data->len();
		if ((char*) (t + sz + 1) >= (char*) t + prev - (kn + dn))
			return false;
		prev -= (short) (kn + dn);
		x.key.copyto((char*) t + prev);
		memcpy((char*) t + prev + kn, x.data, dn);
		// insert into vector
		std::copy_backward(t + position.i, t + sz, t + sz + 1);
		t[position.i] = prev;
		++sz;
		return true;
		}
	void insert(const iterator& position, iterator first, const iterator& last)
		{
		int n = last - first;
		int i = position.i;
		std::copy_backward(t + i, t + sz, t + sz + n);
		for (; first != last; ++first)
			{
			VVslot x = *first;
			int kn = roundup(x.key.cursize());
			int dn = x.data->len();
			ck(kn + dn);
			prev -= (short) (kn + dn);
			x.key.copyto((char*) t + prev);
			memcpy((char*) t + prev + kn, x.data, dn);
			t[i] = prev;
			++sz; ++i;
			}
		}
	void append(const iterator& first, const iterator& last)
		{ insert(end(), first, last); }
	void erase(iterator position)
		{
		// erase from heap
		VVslot x = *position;
		int n = roundup(x.key.cursize()) + x.data->len();
		int offset = t[position.i];
		memmove((char*) t + prev + n, (char*) t + prev, offset - prev);
		prev += n;
		// erase from vector
		std::copy(t + position.i + 1, t + sz, t + position.i);
		t[--sz] = 0;
		// adjust vector
		for (int i = 0; i < sz; ++i)
			if (t[i] < offset)
				t[i] += n;
		}
	void erase(const iterator& first, const iterator& last)
		{
		// this is not the fastest method, but it's easy!
		for (iterator i = last; i != first; --i)
			erase(i - 1);
		}
	int remaining() const
		{ return ((char*) t + prev) - (char*) (t + sz); }
private:
	short t[(mem - sizeof (short) - sizeof (short)) / sizeof (short)];	// array of offsets
	short prev;		// points to start of heap (grows downward)
	short sz;
	short heapsize() const
		{ return sizeof t - prev; }
	void ck(int n)
		{ verify((char*) (t + sz) < (char*) t + prev - n); }
	};
