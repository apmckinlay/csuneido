#ifndef SUOBJECT_H
#define SUOBJECT_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2000 Suneido Software Corp. 
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

#include "value.h"
#include "suvalue.h"
#include <vector>
#include "hashmap.h"

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#endif

// objects - both generic containers,
// and instances of user defined classes
class SuObject : public SuValue
	{
public:
	typedef std::vector<Value> Vector;
	typedef HashMap<Value,Value> Map;

	SuObject();
	explicit SuObject(bool readonly);
	explicit SuObject(const SuObject& ob);
	void init();
	SuObject(SuObject* ob, size_t offset); // for slice 

	virtual void out(Ostream& os);
	void outdelims(Ostream& os, char* delim);
	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	virtual void putdata(Value i, Value x);
	virtual Value getdata(Value);
	virtual SuObject* ob_if_ob()
		{ return this; }

	void set_members(SuObject* x);

	size_t size() const
		{ return vec.size() + map.size(); }
	size_t vecsize() const
		{ return vec.size(); }
	size_t mapsize() const
		{ return map.size(); }
	
	void put(Value i, Value x);

	void add(Value x);

	Value get(Value i) const;

	virtual bool has(Value);

	virtual bool erase(Value x);
	virtual bool erase2(Value x);

	bool operator==(const SuObject& ob) const;
	virtual bool eq(const SuValue& x) const;
	virtual bool lt(const SuValue& y) const;
	virtual int order() const;
	virtual size_t hashfn();

	size_t packsize() const;
	void pack(char* buf) const;
	static SuObject* unpack(const gcstring& s);
	static SuObject* unpack2(const gcstring& s, SuObject* ob);

	SuObject* slice(size_t offset);

	void set_readonly()
		{ readonly = true; }
	bool get_readonly()
		{ return readonly; }

	Value myclass;
	Value defval; // default value

	typedef std::pair<Value,Value> Pair;
	class iterator
		{
	public:
		iterator(const Vector& v, const Map& m, bool iv, bool im, int& ver) 
			: vec(v), vi(iv ? 0 : v.size()), map(m), mi(im ? m.begin() : m.end()), mend(m.end()),
			include_vec(iv), include_map(im), object_version(ver), version(ver)
			{ }
		bool operator==(const iterator& iter) const;
		iterator& operator++();
		Pair operator*();
		void rewind();
		void checkForModification();
	private:
		const Vector& vec;
		size_t vi;
		const Map& map;
		Map::const_iterator mi;
		Map::const_iterator mend;
		bool include_vec, include_map;
		int& object_version;
		int version;
		};
	iterator begin(bool include_vec = true, bool include_map = true)
		{ return iterator(vec, map, include_vec, include_map, version); }
	iterator end()
		{ return iterator(vec, map, false, false, version); }
	Value find(Value value);
	void remove1(Value value);
	void sort();
	bool hasMethod(Value name);
protected:
	typedef Value (SuObject::*pmfn)(short, short, ushort*, int);
	static HashMap<Value,pmfn> basic_methods;
private:
	static HashMap<Value,pmfn> methods;

	void setup();
	Value getdefault(Value member, Value def);
	Value Size(short nargs, short nargnames, ushort* argnames, int each);
	Value Members(short nargs, short nargnames, ushort* argnames, int each);
	Value Iter(short nargs, short nargnames, ushort* argnames, int each);
	Value IterList(short nargs, short nargnames, ushort* argnames, int each);
	Value IterKeys(short nargs, short nargnames, ushort* argnames, int each);
	Value IterListValues(short nargs, short nargnames, ushort* argnames, int each);
	Value HasMember(short nargs, short nargnames, ushort* argnames, int each);
	Value HasMethod(short nargs, short nargnames, ushort* argnames, int each);
	Value MethodClass(short nargs, short nargnames, ushort* argnames, int each);
	Value Base(short nargs, short nargnames, ushort* argnames, int each);
	Value HasBase(short nargs, short nargnames, ushort* argnames, int each);
	Value Eval(short nargs, short nargnames, ushort* argnames, int each);
	Value Eval2(short nargs, short nargnames, ushort* argnames, int each);
	Value Set_default(short nargs, short nargnames, ushort* argnames, int each);
	Value Copy(short nargs, short nargnames, ushort* argnames, int each);
	Value Partition(short nargs, short nargnames, ushort* argnames, int each);
	Value Sort(short nargs, short nargnames, ushort* argnames, int each);
	Value LowerBound(short nargs, short nargnames, ushort* argnames, int each);
	Value UpperBound(short nargs, short nargnames, ushort* argnames, int each);
	Value EqualRange(short nargs, short nargnames, ushort* argnames, int each);
	Value Unique(short nargs, short nargnames, ushort* argnames, int each);
	Value Slice(short nargs, short nargnames, ushort* argnames, int each);
	Value Find(short nargs, short nargnames, ushort* argnames, int each);
	Value Erase(short nargs, short nargnames, ushort* argnames, int each);
	Value Delete(short nargs, short nargnames, ushort* argnames, int each);
	void clear();
	Value Add(short nargs, short nargnames, ushort* argnames, int each);
	Value Reverse(short nargs, short nargnames, ushort* argnames, int each);
	Value Join(short nargs, short nargnames, ushort* argnames, int each);
	Value Set_readonly(short nargs, short nargnames, ushort* argnames, int each);
	void setReadonly();
	Value IsReadonly(short nargs, short nargnames, ushort* argnames, int each);
	Value Values(short nargs, short nargnames, ushort* argnames, int each);
	Value Assocs(short nargs, short nargnames, ushort* argnames, int each);
	Value GetDefault(short nargs, short nargnames, ushort* argnames, int each);

	Vector vec;
	Map map;
	bool readonly;
	// wasteful to put these on every object instead of on class
	// but due to alignment they won't actually take any more space
	bool has_getter;
	bool has_setter;
	int version; // incremented when member is added or removed
	// used to detect modification during iteration
	friend class ModificationCheck;
	};

#endif
