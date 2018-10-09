#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "except.h"
#include "gsl-lite.h"
#include <iterator>

/*
Simple list template similar to std::vector.
Uses uint16_t for size (for compactness) so limit is 64k elements.
cap(acity) is set to USHRT_MAX when read-only.
Intended for garbage collection so no destructor handling.
Unlike vector, assignment and copy constructor do NOT copy.
Instead they set both source and target to readonly and share data.
Move assignment and constructor do not set readonly.
Throws exception for readonly violation.
Keeps elements in added order wherever reasonable.
*/
template <typename T>
class List {
public:
	// empty list, no allocation
	List() = default;

	List(const List& other) : data(other.data), cap(other.cap), siz(other.siz) {
		sharewith(other);
	}
	// move constructor
	List(List&& other) : data(other.data), cap(other.cap), siz(other.siz) {
		other.reset();
	}
	List(std::initializer_list<T> init) {
		cap = siz = init.size();
		data = static_cast<T*>(::operator new(sizeof(T) * cap));
		memcpy(data, init.begin(), sizeof(T) * siz);
	}
	List(T* arr, int n) : data(arr), cap(n), siz(n) {
		make_readonly();
	}

	List& operator=(const List& other) {
		if (&other != this) {
			data = other.data;
			siz = other.siz;
			cap = other.cap;
			sharewith(other);
		}
		return *this;
	}
	List& operator=(List&& other) {
		if (&other != this) {
			data = other.data;
			siz = other.siz;
			cap = other.cap;
			other.reset();
		}
		return *this;
	}

	int size() const {
		return siz;
	}
	bool empty() const {
		return siz == 0;
	}
	bool has(const T& x) const {
		for (auto p = data, end = data + siz; p < end; ++p)
			if (*p == x)
				return true;
		return false;
	}
	// same elements in same order, SLOW O(N)
	bool operator==(const List<T>& other) const {
		if (this == &other)
			return true;
		if (siz != other.siz)
			return false;
		if (data == other.data) // will catch nullptr
			return true;
		return std::equal(begin(), end(), other.begin());
	}

	List& add(const T& x) {
		verify(!readonly());
		if (siz >= cap)
			grow();
		construct(&data[siz++], x);
		return *this;
	}
	// push is a synonym for add
	void push(const T& x) {
		add(x);
	}
	// removes the last element
	void pop() {
		popn(1);
	}
	// removes the last n element
	void popn(int n) {
		verify(!readonly());
		verify(siz >= n);
		siz -= n;
		memset(data + siz, 0, n * sizeof(T)); // for garbage collection
	}
	// SLOW O(N)
	T popfront() {
		verify(!readonly());
		verify(siz > 0);
		auto x = data[0];
		shift(data);
		return x;
	}

	// removes the first occurrence of a value, SLOW O(N)
	bool erase(const T& x) {
		for (auto p = data, end = data + siz; p < end; ++p)
			if (*p == x) {
				shift(p);
				return true;
			}
		return false;
	}
	// delete the element at a given index, shifting the following, SLOW O(N)
	void remove(int i) {
		verify(0 <= i && i < siz);
		shift(data + i);
	}
	// Makes it empty but keeps the current capacity
	List& clear() {
		verify(!readonly());
		siz = 0;
		memset(data, 0, cap * sizeof(T)); // help garbage collection
		return *this;
	}
	// Resets capacity to zero, releases array
	List& reset() {
		data = nullptr;
		cap = siz = 0;
		return *this;
	}
	void swap(List& that) {
		std::swap(data, that.data);
		std::swap(cap, that.cap);
		std::swap(siz, that.siz);
	}

	// returns a move-able copy, shrunk to fit
	List copy() const {
		List<T> tmp;
		tmp.data = new T[siz];
		std::copy(begin(), end(), tmp.data);
		// memcpy would be sufficient if you don't need constructors
		tmp.siz = tmp.cap = siz;
		return tmp;
	}

	T& operator[](int i) { // WARNING: loophole for readonly
		verify(0 <= i && i < siz);
		return data[i];
	}
	const T& operator[](int i) const {
		verify(0 <= i && i < siz);
		return data[i];
	}
	T& back() {
		verify(siz > 0);
		return data[siz - 1];
	}

	auto begin() {
		return gsl::make_span(data, data + siz).begin();
	}
	auto end() {
		return gsl::make_span(data, data + siz).end();
	}
	auto begin() const {
		return gsl::make_span(data, data + siz).begin();
	}
	auto end() const {
		return gsl::make_span(data, data + siz).end();
	}

private:
	void grow() {
		if (cap < 4)
			cap = 4;
		else if (cap < MAXCAP / 2)
			cap = 2 * cap;
		else if (cap < MAXCAP)
			cap = MAXCAP;
		else
			except("list cannot exceed " << MAXCAP << " elements");
		T* d = static_cast<T*>(::operator new(sizeof(T) * cap));
		memcpy(d, data, sizeof(T) * siz);
		data = d;
	}
	void shift(T* p) {
		verify(!readonly());
		auto end = data + siz;
		if (p + 1 < end)
			memcpy(p, p + 1, (end - p - 1) * sizeof(T));
		memset(end - 1, 0, sizeof(T)); // for garbage collection
		--siz;
	}
	void make_readonly() const {
		cap = READONLY;
	}
	void sharewith(const List& other) const {
		if (!data)
			return;
		make_readonly();
		other.make_readonly();
	}
	bool readonly() {
		return cap == READONLY;
	}

	T* data = nullptr;
	mutable uint16_t cap = 0; // capacity of data, max for readonly
	uint16_t siz = 0;         // current number of elements
	static const uint16_t READONLY = USHRT_MAX;
	static const uint16_t MAXCAP = USHRT_MAX - 1;
};

#ifndef _WIN64
static_assert(sizeof(List<int>) == 8);
#endif

// ------------------------------------------------------------------

// Simple set based on List.
// WARNING: Not for large sets - add() and has() are O(N)
template <typename T>
class ListSet {
public:
	ListSet() = default;
	ListSet(const ListSet& other) : list(other.list) {
	}
	ListSet(ListSet&& other) : list(other.list) {
	}

	ListSet& operator=(const ListSet& other) {
		list = other.list;
		return *this;
	}
	ListSet& operator=(ListSet&& other) {
		list = other.list;
		return *this;
	}

	int size() {
		return list.size();
	}
	bool empty() {
		return list.empty();
	}
	// SLOW O(N)
	bool has(const T& x) {
		return list.has(x);
	}
	// SLOW O(N)
	ListSet& add(const T& x) {
		if (!has(x))
			list.add(x);
		return *this;
	}
	T popfront() {
		return list.popfront();
	}
	ListSet copy() const {
		return ListSet(list);
	}
	ListSet& reset() {
		list.reset();
		return *this;
	}
	ListSet& clear() {
		list.clear();
		return *this;
	}

	// only const iterator so you can't break uniqueness
	auto begin() const {
		return list.begin();
	}
	auto end() const {
		return list.end();
	}

private:
	ListSet(const List<T>& v) : list(v.copy()) { // for copy
	}

	List<T> list;
};

#include "ostream.h"

template <class T>
Ostream& operator<<(Ostream& os, const List<T>& list) {
	os << '(';
	auto sep = "";
	for (auto x : list) {
		os << sep << x;
		sep = ",";
	}
	return os << ')';
}
