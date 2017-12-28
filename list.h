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
#include "gsl-lite.h"
#include <iterator>

/*
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
	// empty list, no allocation
	List() = default;

	List(const List& other) : data(other.data), cap(other.cap), siz(other.siz)
		{
		if (data)
			readonly = other.readonly = true;
		}
	List(List&& other) : data(other.data), cap(other.cap), siz(other.siz)
		{
		other.reset();
		}
	List(std::initializer_list<T> init)
		{
		cap = siz = init.size();
		data = static_cast<T*>(::operator new (sizeof(T) * cap));
		memcpy(data, init.begin(), sizeof(T) * siz);
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
	// same elements in same order
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
		memset(data + siz, 0, sizeof (T)); // for garbage collection
		}
	T popfront()
		{
		verify(siz > 0);
		auto x = data[0];
		shift(data);
		return x;
		}

	// removes the first occurrence of a value
	bool erase(const T& x)
		{
		for (auto p = data, end = data + siz; p < end; ++p)
			if (*p == x)
				{
				shift(p);
				return true;
				}
		return false;
		}
	// Makes it empty but keeps the current capacity
	List& clear()
		{
		verify(!readonly);
		siz = 0;
		memset(data, 0, cap * sizeof(T)); // help garbage collection
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

	auto begin()
		{ return gsl::make_span(data, data + siz).begin(); }
	auto end()
		{ return gsl::make_span(data, data + siz).end(); }
	auto begin() const
		{ return gsl::make_span(data, data + siz).begin(); }
	auto end() const
		{ return gsl::make_span(data, data + siz).end(); }

private:
	void grow()
		{
		cap = (cap == 0) ? 8 : 2 * cap;
		T* d = static_cast<T*>(::operator new (sizeof(T) * cap));
		memcpy(d, data, sizeof(T) * siz);
		data = d;
		}
	void shift(T* p)
		{
		auto end = data + siz;
		if (p + 1 < end)
			memcpy(p, p + 1, (end - p - 1) * sizeof(T));
		memset(end - 1, 0, sizeof(T)); // for garbage collection
		--siz;
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
	T popfront()
		{ return list.popfront(); }
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
	auto begin() const
		{ return list.begin(); }
	auto end() const
		{ return list.end(); }

private:
	ListSet(const List<T>& v) : list(v.copy()) // for copy
		{}

	List<T> list;
	};

#include "ostream.h"

template <class T> Ostream& operator<<(Ostream& os, const List<T>& list)
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
