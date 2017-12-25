#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation - version 2.
*
* This program is distributed in the hope that it will be
* useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE.  See the GNU General Public License in the file COPYING
* for more details.
*
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the Free
* Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "except.h"
#include <iterator>

/* ==================================================================
 * Simple generic pointer random access iterator. No bounds checking.
 */
template <typename T>
class PtrIter
	{
public:
	using value_type = T;
	using difference_type = std::ptrdiff_t;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	PtrIter() = default;
	explicit PtrIter(pointer p) : ptr(p)
		{}

	reference operator*() const
		{ return *ptr; }
	pointer operator->() const 
		{ return ptr; }
	reference operator[](difference_type rhs) const 
		{ return ptr[rhs]; }

	PtrIter& operator++()
		{ ++ptr; return *this; }
	PtrIter operator++(int)
		{ return PtrIter(ptr++); }

	PtrIter& operator--()
		{ --ptr; return *this; }
	PtrIter operator--(int)
		{ return PtrIter(ptr++); }

	PtrIter& operator+=(difference_type rhs) // all other +/- offset go thru here
		{ ptr += rhs; return *this; }
	PtrIter& operator-=(difference_type rhs) 
		{ *this += -rhs; return *this; }

	PtrIter operator+(difference_type rhs) const 
		{ return PtrIter(ptr) += rhs; }
	PtrIter operator-(difference_type rhs) const 
		{ return PtrIter(ptr) += -rhs; }
	friend PtrIter operator+(difference_type lhs, PtrIter rhs) 
		{ return rhs += lhs; }

	difference_type operator-(const PtrIter& rhs) const 
		{ return ptr - rhs.ptr; }

	bool operator==(const PtrIter& rhs) const
		{ return ptr == rhs.ptr; }
	bool operator!=(const PtrIter& rhs) const
		{ return ptr != rhs.ptr; }
	bool operator>(const PtrIter& rhs) const
		{ return ptr > rhs.ptr; }
	bool operator<(const PtrIter& rhs) const 
		{ return ptr < rhs.ptr; }
	bool operator>=(const PtrIter& rhs) const 
		{ return ptr >= rhs.ptr; }
	bool operator<=(const PtrIter& rhs) const 
		{ return ptr <= rhs.ptr; }

private:
	T* ptr = nullptr;
	};

/* ==================================================================
 * Simple list template similar to std::vector.
 * Intended for garbage collection so no destructor handling.
 * Unlike vector, assignment and copy constructor do NOT copy.
 * Instead they set both source and target to readonly and share data.
 * Move assignment and copy constructor do not set readonly.
 * Throws exception for readonly violation.
 * Keeps elements in added order wherever reasonable.
 */
template <typename T>
class List
	{
public:
	using iterator = PtrIter<T>;
	using const_iterator = PtrIter<const T>;

	// empty list, no allocation
	List() = default;

	List(const List& other) 
		: data(other.data), siz(other.siz), cap(other.cap)
		{
		if (data)
			readonly = other.readonly = true;
		}
	List(List&& other) : data(other.data), siz(other.siz), cap(other.cap)
		{
		other.reset();
		}

	List& operator=(const List& other) 
		{
		if (&other != this)
			{
			data = other.data;
			siz = other.siz;
			cap = other.cap;
			if (data)
				readonly = other.readonly = true;
			}
		return *this;
		}
	List& operator=(List&& other) 
		{
		if (&other != this)
			{
			data = other.data;
			siz = other.siz;
			cap = other.cap;
			other.reset();
			}
		return *this;
		}

	int size() const
		{ return siz; }
	bool empty() const
		{ return siz == 0; }
	bool has(const T& x) const
		{
		for (auto p = data, end = data + siz; p < end; ++p)
			if (*p == x)
				return true;
		return false;
		}
	bool operator==(const List<T>& other) const
		{
		if (this == &other)
			return true;
		if (siz != other.siz)
			return false;
		if (data == other.data) // will catch nullptr
			return true;
		return std::equal(begin(), end(), other.begin());
		}

	List& add(const T& x)
		{
		verify(!readonly);
		if (siz >= cap)
			grow();
		construct(&data[siz++], x);
		return *this;
		}
	// push is a synonym for add
	void push(const T& x)
		{ add(x); }
	// removes the last element
	void pop()
		{
		verify(siz > 0);
		--siz;
		}
	// Makes it empty but keeps the current capacity
	List& clear()
		{
		verify(!readonly);
		siz = 0;
		return *this;
		}
	// Resets capacity to zero, releases array
	List& reset()
		{
		data = nullptr;
		cap = siz = 0;
		readonly = false;
		return *this;
		}

	// returns a move-able copy, shrunk to fit
	List copy() const
		{
		List<T> tmp;
		tmp.data = new T[siz];
		std::copy(begin(), end(), tmp.data);
		// memcpy would be sufficient if you don't need constructors
		tmp.siz = tmp.cap = siz;
		return tmp;
		}

	T& operator[](int i) // WARNING: loophole for readonly
		{
		verify(0 <= i && i < siz);
		return data[i];
		}
	const T& operator[](int i) const
		{
		verify(0 <= i && i < siz);
		return data[i];
		}

	iterator begin()
		{ return iterator(data); }
	iterator end()
		{ return iterator(data + siz); }
	const_iterator begin() const
		{ return const_iterator(data); }
	const_iterator end() const
		{ return const_iterator(data + siz); }

private:
	void List<T>::grow()
		{
		cap = (cap == 0) ? 8 : 2 * cap;
		T* d = static_cast<T*>(::operator new (sizeof(T) * cap));
		memcpy(d, data, sizeof(T) * siz);
		data = d;
		}

	T* data = nullptr;
	int cap = 0; // size of data
	int siz = 0; // current number of elements
	mutable bool readonly = false;
	};

/* ==================================================================
 * Simple set based on List.
 * WARNING: Not for large sets - add() and has() are O(N)
 */
template <typename T>
class ListSet
	{
public:
	using const_iterator = PtrIter<const T>;

	ListSet() = default;
	ListSet(const ListSet& other) : list(other.list)
		{}
	ListSet(ListSet&& other) : list(other.list)
		{}

	ListSet& operator=(const ListSet& other)
		{
		list = other.list;
		return *this;
		}
	ListSet& operator=(ListSet&& other)
		{
		list = other.list;
		return *this;
		}

	int size()
		{ return list.size(); }
	bool empty()
		{ return list.empty(); }
	bool has(const T& x)
		{ return list.has(x); }
	ListSet& add(const T& x)
		{
		if (!has(x))
			list.add(x);
		return *this;
		}
	ListSet copy() const
		{ return ListSet(list); }
	ListSet& reset()
		{
		list.reset();
		return *this;
		}
	ListSet& clear()
		{
		list.clear();
		return *this;
		}

	// only const iterator so you can't break uniqueness
	const_iterator begin() const
		{ return list.begin(); }
	const_iterator end() const
		{ return list.end(); }

private:
	ListSet(const List<T>& v) : list(v.copy()) // for copy
		{}

	List<T> list;
	};

#include "ostream.h"

template <class T> Ostream& operator<<(Ostream& os, List<T> list)
	{
	os << '(';
	auto sep = "";
	for (auto x : list)
		{
		os << sep << x;
		sep = ",";
		}
	return os << ')';
	}
