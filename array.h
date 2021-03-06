#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <algorithm>
#include "except.h"

using namespace std;

// TODO: replace with std::array

template <class T, int cap>
class Array {
public:
	Array() {
		sz = 0;
	}
	T& operator[](int i) {
		verify(i < cap);
		return t[i];
	}
	typedef T* iterator;
	typedef const T* const_iterator;
	static int capacity() {
		return cap;
	}
	int size() const {
		return sz;
	}
	bool empty() const {
		return sz == 0;
	}
	Array& operator=(const Array&);
	void erase(int i) {
		erase(begin() + i);
	}
	void erase(iterator position) {
		std::copy(position + 1, end(), position);
		--sz;
	}
	void erase(iterator first, iterator last) {
		std::copy(last, end(), first);
		sz -= (int) (last - first);
	}
	iterator begin() {
		return t;
	}
	iterator end() {
		return t + sz;
	}
	const_iterator begin() const {
		return t;
	}
	const_iterator end() const {
		return t + sz;
	}
	T& front() {
		return t[0];
	}
	T& back() {
		verify(sz > 0);
		return t[sz - 1];
	}
	void pop_back() {
		verify(sz > 0);
		--sz;
	}
	void insert(const T& x) {
		insert(begin(), x);
	}
	bool insert(iterator position, const T& x);
	void insert(iterator position, int n, const T& x);
	void insert(iterator position, iterator first, iterator last);
	void push_back(const T& x) {
		verify(sz < cap);
		t[sz++] = x;
	}
	void append(const T& x) {
		insert(end(), x);
	}
	void append(int n, const T& x) {
		insert(end(), n, x);
	}
	void append(iterator first, iterator last) {
		insert(end(), first, last);
	}
	T* dup();
	int remaining() const {
		return cap - sz;
	}

protected:
	T t[cap];
	int sz; // current used size
};

template <class T, int cap>
Array<T, cap>& Array<T, cap>::operator=(const Array& v) {
	verify(cap >= v.size());
	std::copy(v.begin(), v.end(), begin());
	sz = v.size();
	return *this;
}

template <class T, int cap>
bool Array<T, cap>::insert(iterator position, const T& x) {
	if (sz + 1 > cap)
		return false;
	std::copy_backward(position, end(), end() + 1);
	*position = x;
	++sz;
	return true;
}

template <class T, int cap>
void Array<T, cap>::insert(iterator position, int n, const T& x) {
	verify(sz + n <= cap);
	copy_backward(position, end(), end() + n);
	fill(position, position + n, x);
	sz += n;
}

template <class T, int cap>
void Array<T, cap>::insert(iterator position, iterator first, iterator last) {
	auto n = last - first;
	verify(sz + n <= cap);
	std::copy_backward(position, end(), end() + n);
	std::copy(first, last, position);
	sz += n;
}

template <class T, int cap>
T* Array<T, cap>::dup() {
	T* a = new T[sz];
	std::copy(begin(), end(), a);
	return a;
}
