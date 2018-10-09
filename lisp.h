#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include "except.h"

// TODO add begin/end for standard C++ iteration

template <class T>
class Lisp;

template <class T>
Lisp<T> lisp(const T& x1);
template <class T>
Lisp<T> lisp(const T& x1, const T& x2);
template <class T>
Lisp<T> lisp(const T& x1, const T& x2, const T& x3);
template <class T>
Lisp<T> lisp(const T& x1, const T& x2, const T& x3, const T& x4);

template <class T>
Lisp<T> lispn(const T& x, int n);

template <class T>
Lisp<T> cons(const T& t);
template <class T>
Lisp<T> cons(const T& t, const Lisp<T>& list);

// single linked homogenous list template
template <class T>
class Lisp {
private:
	// value + next pointer
	struct Cons {
		T value;
		Cons* next;
		explicit Cons(const T& v, Cons* n = 0) : value(v), next(n) {
		}
	};
	explicit Lisp(Cons* c) : first(c) {
	}
	static Cons* _cons(const T& t, Cons* c = 0) {
		return new Cons(t, c);
	}

public:
	Lisp() : first(0) {
	}
	explicit Lisp(const T& x1) {
		first = _cons(x1);
	}
	Lisp(const T& x1, const T& x2) {
		first = _cons(x1, _cons(x2));
	}
	Lisp(const T& x1, const T& x2, const T& x3) {
		first = _cons(x1, _cons(x2, _cons(x3)));
	}
	Lisp(const T& x1, const T& x2, const T& x3, const T& x4) {
		first = _cons(x1, _cons(x2, _cons(x3, _cons(x4))));
	}
	friend Lisp lisp<>(const T& x1);
	friend Lisp lisp<>(const T& x1, const T& x2);
	friend Lisp lisp<>(const T& x1, const T& x2, const T& x3);
	friend Lisp lisp<>(const T& x1, const T& x2, const T& x3, const T& x4);
	friend Lisp lispn<>(const T& x, int n);
	bool operator==(const Lisp& yy) const {
		if (first == yy.first)
			return true;
		Lisp x = *this, y = yy;
		for (; !nil(x) && !nil(y); ++x, ++y)
			if (car(x) != car(y))
				return false;
		return nil(x) && nil(y);
	}
	friend bool operator!=(const Lisp& x, const Lisp& y) {
		return !(x == y);
	}
	Lisp& push(const T& t) {
		first = _cons(t, first);
		return *this;
	}
	void pop() {
		if (first)
			first = first->next;
	}
	// WARNING: append mutates the tail of the list, it's not functional
	Lisp& append(const T& t) {
		if (!first)
			first = _cons(t);
		else {
			Cons* x = first;
			for (; x->next; x = x->next)
				;
			x->next = _cons(t);
		}
		return *this;
	}
	friend bool nil(const Lisp& list) {
		return list.first == 0;
	}
	friend T& car(const Lisp& list) {
		verify(list.first);
		return list.first->value;
	}
	T& operator*() const {
		verify(first);
		return first->value;
	}
	T* operator->() {
		verify(first);
		return &first->value;
	}
	friend Lisp cdr(const Lisp& list) {
		verify(list.first);
		return Lisp<T>(list.first->next);
	}
	Lisp& operator++() {
		if (first)
			first = first->next;
		return *this;
	}
	Lisp operator++(int) {
		Lisp ret = *this;
		if (first)
			first = first->next;
		return ret;
	}
	friend Lisp cons<>(const T& t);
	friend Lisp cons<>(const T& t, const Lisp& list);
	friend int size(Lisp list) {
		return list.size();
	}
	bool empty() const {
		return !first;
	}
	int size() const {
		int n = 0;
		for (Cons* p = first; p; p = p->next)
			++n;
		return n;
	}
	T& operator[](int i) {
		return nth(i);
	}
	const T& operator[](int i) const {
		return nth(i);
	}
	T& nth(int i) {
		verify(i >= 0 && first);
		Cons* p = first;
		for (; p->next && i > 0; p = p->next, --i)
			;
		if (i != 0)
			except("attempted access past end of list");
		return p->value;
	}
	const T& nth(int i) const {
		verify(i >= 0 && first);
		Cons* p = first;
		for (; p->next && i > 0; p = p->next, --i)
			;
		if (i != 0)
			except("attempted access past end of list");
		return p->value;
	}
	Lisp nthcdr(int i) const {
		Cons* p = first;
		for (; p && i > 0; p = p->next, --i)
			;
		if (i != 0)
			except("attempted access past end of list");
		return Lisp<T>(p);
	}
	bool member(const T& x) const {
		for (Cons* p = first; p; p = p->next)
			if (p->value == x)
				return true;
		return false;
	}
	Lisp& reverse() {
		Cons* prev = 0;
		for (Cons* p = first; p;) {
			Cons* next = p->next;
			p->next = prev;
			prev = p;
			p = next;
		}
		first = prev;
		return *this;
	}
	Lisp& erase(const T& x) {
		if (!first)
			return *this;
		if (first->value == x) {
			first = first->next;
			return *this;
		}
		for (Cons *prev = first, *p = first->next; p; prev = p, p = p->next)
			if (p->value == x) {
				prev->next = p->next;
				break;
			}
		return *this;
	}
	Lisp& sort() {
		if (!first || !first->next)
			return *this;
		Lisp<T> x = split();
		sort();
		x.sort();
		merge(x);
		return *this;
	}
	Lisp copy() const {
		Lisp copy;
		Cons** y = &copy.first;
		for (const Cons* p = first; p; p = p->next) {
			*y = new Cons(p->value, 0);
			y = &((*y)->next);
		}
		return copy;
	}

private:
	Cons* first;
	// split chops off the second half of the list and returns it
	Lisp split() {
		if (!first)
			return Lisp<T>();
		Cons* mid = first;
		Cons* p = first;
		while (p) {
			p = p->next;
			if (p)
				p = p->next;
			if (p)
				mid = mid->next;
		}
		p = mid->next;
		mid->next = 0;
		return Lisp<T>(p);
	}
	void merge(Lisp y) {
		Lisp x = *this;
		Cons** p = &first;
		while (!nil(x) || !nil(y)) {
			if (nil(x) || (!nil(y) && *y < *x)) {
				*p = y.first;
				++y;
			} else {
				*p = x.first;
				++x;
			}
			p = &(*p)->next;
		}
		*p = 0;
	}

public:
	void* first_() const { // for use by HashFirst and EqFirst
		return (void*) first;
	}
};

template <class T>
Lisp<T> lisp(const T& x1) {
	return Lisp<T>(Lisp<T>::_cons(x1));
}
template <class T>
Lisp<T> lisp(const T& x1, const T& x2) {
	return Lisp<T>(Lisp<T>::_cons(x1, Lisp<T>::_cons(x2)));
}
template <class T>
Lisp<T> lisp(const T& x1, const T& x2, const T& x3) {
	return Lisp<T>(Lisp<T>::_cons(x1, Lisp<T>::_cons(x2, Lisp<T>::_cons(x3))));
}
template <class T>
Lisp<T> lisp(const T& x1, const T& x2, const T& x3, const T& x4) {
	return Lisp<T>(Lisp<T>::_cons(
		x1, Lisp<T>::_cons(x2, Lisp<T>::_cons(x3, Lisp<T>::_cons(x4)))));
}

template <class T>
Lisp<T> lispn(const T& x, int n) {
	Lisp<T> list;
	while (n-- > 0)
		list.push(x);
	return list;
}

template <class T>
Lisp<T> cons(const T& t) {
	return Lisp<T>(new typename Lisp<T>::Cons(t, 0));
}
template <class T>
Lisp<T> cons(const T& t, const Lisp<T>& list) {
	return Lisp<T>(new typename Lisp<T>::Cons(t, list.first));
}

#include <cstddef> // for size_t

// hash the list pointer NOT the values, used by Select
template <class T>
struct HashFirst {
	size_t operator()(const T& x) {
		return (size_t) x.first_();
	}
};

// compare the list pointer NOT the values, used by Select
template <class T>
struct EqFirst {
	bool operator()(const T& x, const T& y) {
		return x.first_() == y.first_();
	}
};

// return a copy of the list in reverse order
template <class T>
Lisp<T> reverse(const Lisp<T>& list) {
	Lisp<T> rev;
	for (Lisp<T> i = list; !nil(i); ++i)
		rev = cons(car(i), rev);
	return rev;
}

// return true if the value is in the list
template <class T>
bool member(Lisp<T> list, const T& t) {
	for (; !nil(list) && car(list) != t; list = cdr(list))
		;
	return !nil(list);
}

// return the index of the value in the list
// or -1 if the value isnt found
template <class T1, class T2>
int search(Lisp<T1> list, const T2& t) {
	int i;
	for (i = 0; !nil(list) && car(list) != t; list = cdr(list), ++i)
		;
	return nil(list) ? -1 : i;
}

// return the tail of the list starting with the value
// or an empty list if the value isnt found
template <class T1, class T2>
Lisp<T1> find(Lisp<T1> list, const T2& t) {
	for (; !nil(list) && *list != t; ++list)
		;
	return list;
}

// return a copy of the list with all occurrences of a value removed
template <class T>
Lisp<T> erase(Lisp<T> list, const T& t) {
	Lisp<T> result;
	for (; !nil(list); list = cdr(list))
		if (car(list) != t)
			result.push(car(list));
	return result.reverse();
}

// return true is y is a prefix of x
template <class T>
bool prefix(Lisp<T> x, Lisp<T> y) {
	for (; !nil(y); x = cdr(x), y = cdr(y))
		if (nil(x) || car(x) != car(y))
			return false;
	return true;
}

// return a list that is the concatenation of two lists
template <class T>
Lisp<T> concat(Lisp<T> x, Lisp<T> y) {
	Lisp<T> list;
	for (; !nil(x); x = cdr(x))
		list = cons(car(x), list);
	for (; !nil(y); y = cdr(y))
		list = cons(car(y), list);
	return list.reverse();
}

// return a list of values that are in both x and y
template <class T>
Lisp<T> intersect(Lisp<T> x, const Lisp<T>& y) {
	Lisp<T> list;
	for (; !nil(x); x = cdr(x)) {
		T& t = car(x);
		for (Lisp<T> z(y); !nil(z); z = cdr(z))
			if (t == car(z))
				list = cons(t, list);
	}
	return list.reverse();
}

// return a list of values in x that aren't in y
template <class T>
Lisp<T> difference(Lisp<T> x, const Lisp<T>& y) {
	Lisp<T> list;
	for (; !nil(x); x = cdr(x)) {
		T& t = car(x);
		if (!member(y, t))
			list.push(t);
	}
	return list.reverse();
}

// return a copy of the list with duplicates eliminated
template <class T>
Lisp<T> lispset(Lisp<T> x) {
	Lisp<T> list;
	for (; !nil(x); x = cdr(x)) {
		T& t = car(x);
		Lisp<T> z(list);
		for (; !nil(z); z = cdr(z))
			if (t == car(z))
				break;
		if (nil(z))
			list = cons(t, list);
	}
	return list.reverse();
}

// return true if the values in y are a subset of the values in x
template <class T>
bool subset(const Lisp<T>& x, Lisp<T> y) {
	for (; !nil(y); y = cdr(y))
		if (!member(x, car(y)))
			return false;
	return true;
}

// return a list with all the values from x and y
template <class T>
Lisp<T> set_union(const Lisp<T>& x, Lisp<T> y) {
	Lisp<T> list = reverse(x);
	for (; !nil(y); y = cdr(y)) {
		T& t = car(y);
		Lisp<T> z(x);
		for (; !nil(z); z = cdr(z))
			if (t == car(z))
				break;
		if (nil(z))
			list = cons(t, list);
	}
	return list.reverse();
}

// return true if x and y have the same values
// assumes sets i.e. no duplicates
template <class T>
bool set_eq(const Lisp<T>& x, const Lisp<T>& y) {
	int xn = size(x);
	return size(y) == xn && size(intersect(x, y)) == xn;
}

// return true if list has set as a prefix (in any order)
template <class T>
bool prefix_set(Lisp<T> list, const Lisp<T>& set) {
	int n = size(set);
	int i = 0;
	for (; i < n && !nil(list); list = cdr(list), ++i)
		if (!member(set, car(list)))
			break;
	return i == n;
}

// output a list to a stream
template <class T>
Ostream& operator<<(Ostream& os, Lisp<T> list) {
	os << '(';
	for (; !nil(list); ++list) {
		os << *list;
		if (!nil(cdr(list)))
			os << ',';
	}
	return os << ')';
}

// sorting ----------------------------------------------------------

// used by sort
template <class T>
void split(Lisp<T> z, Lisp<T>& x, Lisp<T>& y) {
	while (!nil(z)) {
		x.push(*z++);
		if (!nil(z))
			y.push(*z++);
	}
}

// used by sort
template <class T>
Lisp<T> merge(Lisp<T> x, Lisp<T> y) {
	if (nil(x))
		return y;
	if (nil(y))
		return x;
	Lisp<T> z;
	while (!nil(x) || !nil(y)) {
		if (nil(x) || (!nil(y) && *y < *x))
			z.push(*y++);
		else
			z.push(*x++);
	}
	return z.reverse();
}

// merge sort a list
template <class T>
Lisp<T> sort(const Lisp<T>& x) {
	if (nil(x) || nil(cdr(x)))
		return x;
	Lisp<T> part1, part2;
	split(x, part1, part2);
	return merge(sort(part1), sort(part2));
}
