#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include <algorithm>
#include "except.h"
#include "hashfn.h"
#include "keyeq.h"
#include "hashsize.h"

// used by HashMap and SuSymbol
template <class Key, class Val, class KofV, class Hfn = HashFn<Key>,
	class Keq = KeyEq<Key> >
class Hashtbl {
private:
	struct Node;

public:
	explicit Hashtbl(int n = 0) { // NOLINT
		init(n);
	}
	void init(int n);
	void clear();
	Hashtbl(const Hashtbl& h);

	Val& insert(const Val& val);
	Val* find(const Key& key) const;
	bool erase(const Key& key);

	int size() const {
		return sz;
	}
	bool empty() const {
		return sz == 0;
	}
	int capacity() const {
		return 2 * cap;
	}
	bool operator==(const Hashtbl& h) const;

	// iterator -----------------------------------------------
	class iterator;
	friend class iterator;
	class iterator {
	public:
		iterator() : tbl(nullptr), node(nullptr), i(-1) {
		}
		iterator(Node** tbl_, int i_) : tbl(tbl_), i(i_) {
			for (--i; i >= 0 && !tbl[i]; --i)
				;
			node = i >= 0 ? tbl[i] : 0;
		}
		Val& operator*() {
			verify(node);
			return node->val;
		}
		Val* operator->() {
			verify(node);
			return &node->val;
		}
		iterator& operator++() {
			verify(tbl);
			verify(node);
			node = node->next;
			if (!node) {
				for (--i; i >= 0 && !tbl[i]; --i)
					;
				if (i >= 0)
					node = tbl[i];
			}
			return *this;
		}
		bool operator==(const iterator& iter) const {
			return node == iter.node;
		}

	protected:
		Node** tbl;
		Node* node;
		int i;
	};
	iterator begin() {
		return iterator(tbl, cap);
	}
	iterator end() {
		return iterator();
	}

	// const_iterator -----------------------------------------------
	class const_iterator;
	friend class const_iterator;
	class const_iterator {
	public:
		const_iterator() : tbl(nullptr), node(nullptr), i(-1) {
		}
		const_iterator(Node** tbl_, int i_) : tbl(tbl_), i(i_) {
			for (--i; i >= 0 && !tbl[i]; --i)
				;
			node = i >= 0 ? tbl[i] : 0;
		}
		Val& operator*() {
			verify(node);
			return node->val;
		}
		Val* operator->() {
			verify(node);
			return &node->val;
		}
		const_iterator& operator++() {
			verify(tbl);
			verify(node);
			node = node->next;
			if (!node) {
				for (--i; i >= 0 && !tbl[i]; --i)
					;
				if (i >= 0)
					node = tbl[i];
			}
			return *this;
		}
		bool operator==(const const_iterator& iter) const {
			return node == iter.node;
		}

	protected:
		Node** tbl;
		Node* node;
		int i;
	};
	const_iterator begin() const {
		return const_iterator(tbl, cap);
	}
	const_iterator end() const {
		return const_iterator();
	}

private:
	// value + next pointer
	struct Node {
		Node* next;
		Val val;
	};
	Node** tbl;  // this is the actual hash table
	Node* nodes; // these are the chain nodes
	Node* free;  // the node free list
	int sz;      // number of Vals in table
	int capi;    // hash_sizes[capi] == cap
	int cap;     // size of hash table, actual capacity is 2 * cap

	Node* allocnode();
	void freenode(Node* node) {
		node->next = free;
		free = node;
	}
	void resize(int ci);
	void swap(Hashtbl<Key, Val, KofV, Hfn, Keq>& x) noexcept;
	int memsize() const {
		return cap * sizeof(Node**) + 2 * cap * sizeof(Node);
	}
};

template <class Key, class Val, class KofV, class Hfn, class Keq>
void Hashtbl<Key, Val, KofV, Hfn, Keq>::init(int n) {
	sz = 0;
	free = 0;
	if (n == 0) {
		cap = capi = 0;
		tbl = nullptr;
		nodes = free = nullptr;
		return;
	}
	n = (n + 1) / 2;
	verify(n < hash_size[n_hash_sizes - 1]);
	for (capi = 0; n > hash_size[capi]; ++capi)
		;
	cap = hash_size[capi];
	char* p = new char[memsize()];
	tbl = (Node**) p;
	std::fill(tbl, tbl + cap, nullptr);
	nodes = (Node*) (p + cap * sizeof(Node**));
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
void Hashtbl<Key, Val, KofV, Hfn, Keq>::clear() {
	sz = 0;
	free = 0;
	memset(tbl, 0, memsize());
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
Hashtbl<Key, Val, KofV, Hfn, Keq>::Hashtbl(const Hashtbl& h) {
	init(h.sz);
	for (const_iterator p = h.begin(); p != h.end(); ++p) {
		// fast version of insert - no check for duplicates, no resize
		int i = Hfn()(KofV()(*p)) % cap;
		Node* node = &nodes[sz++];
		node->next = tbl[i];
		tbl[i] = node;
		construct(&node->val, *p);
	}
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
Val& Hashtbl<Key, Val, KofV, Hfn, Keq>::insert(const Val& val) {
	if (cap == 0)
		resize(capi + 1);
	size_t h = Hfn()(KofV()(val));
	int i = h % cap;
	Node* node = tbl[i];
	for (; node; node = node->next)
		if (Keq()(KofV()(node->val), KofV()(val)))
			return node->val;
	// not found
	if (sz >= 2 * cap) {
		resize(capi + 1);
		i = h % cap;
	}
	node = allocnode();
	node->next = tbl[i];
	tbl[i] = node;
	++sz;
	construct(&node->val, val);
	return node->val;
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
Val* Hashtbl<Key, Val, KofV, Hfn, Keq>::find(const Key& key) const {
	if (sz == 0)
		return nullptr;
	for (Node* node = tbl[Hfn()(key) % cap]; node; node = node->next)
		if (Keq()(KofV()(node->val), key))
			return &node->val;
	return nullptr;
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
bool Hashtbl<Key, Val, KofV, Hfn, Keq>::erase(const Key& key) {
	if (sz == 0)
		return false;
	int i = Hfn()(key) % cap;
	Node** link = &tbl[i];
	for (Node* node = tbl[i]; node; link = &node->next, node = *link)
		if (Keq()(KofV()(node->val), key)) {
			*link = node->next;
			destroy(&node->val);
			memset(&node->val, 0, sizeof node->val);
			freenode(node);
			--sz;
			// if (sz < cap / 2)	// ie. under 1/4 full
			//		resize(capi - 1);	// shrink
			return true;
		}
	return false;
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
bool Hashtbl<Key, Val, KofV, Hfn, Keq>::operator==(
	const Hashtbl<Key, Val, KofV, Hfn, Keq>& h) const {
	if (size() != h.size())
		return false;
	for (const_iterator i = begin(); i != end(); ++i) {
		Val* t = h.find(KofV()(*i));
		if (!t || *t != *i)
			return false;
	}
	return true;
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
typename Hashtbl<Key, Val, KofV, Hfn, Keq>::Node*
Hashtbl<Key, Val, KofV, Hfn, Keq>::allocnode() {
	if (free) {
		Node* node = free;
		free = node->next;
		return node;
	} else {
		verify(sz < 2 * cap);
		return &nodes[sz];
	}
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
void Hashtbl<Key, Val, KofV, Hfn, Keq>::resize(int ci) {
	if (ci == 0) {
		if (sz == 0)
			cap = 0;
		return;
	}
	Hashtbl<Key, Val, KofV, Hfn, Keq> tmp(hash_size[ci] * 2);
	verify(capacity() != tmp.capacity());
	swap(tmp);
	for (iterator p = tmp.begin(); p != tmp.end(); ++p) {
		// fast version of insert - no check for duplicates, no resize
		int i = Hfn()(KofV()(*p)) % cap;
		Node* node = &nodes[sz++];
		node->next = tbl[i];
		tbl[i] = node;
		construct(&node->val, *p);
	}
	verify(size() == tmp.size());
	tmp.clear(); // to aid garbage collection
}

template <class Key, class Val, class KofV, class Hfn, class Keq>
void Hashtbl<Key, Val, KofV, Hfn, Keq>::swap(
	Hashtbl<Key, Val, KofV, Hfn, Keq>& x) noexcept {
	std::swap(sz, x.sz);
	std::swap(cap, x.cap);
	std::swap(capi, x.capi);
	std::swap(tbl, x.tbl);
	std::swap(nodes, x.nodes);
	std::swap(free, x.free);
}
