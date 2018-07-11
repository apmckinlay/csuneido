#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "suvalue.h"
#include <vector>
#include "hashmap.h"
#include "meth.h"
#include "gsl-lite.h"

// generic containers, see also SuInstance and SuClass
class SuObject : public SuValue
	{
public:
	typedef std::vector<Value> Vector;
	typedef HashMap<Value,Value> Map;
	typedef MemFun<SuObject> Mfn;

	SuObject() = default;
	explicit SuObject(bool ro);
	explicit SuObject(const SuObject& ob);
	SuObject(SuObject* ob, size_t offset); // for slice

	void out(Ostream& os) const override;
	void outdelims(Ostream& os, const char* delims) const;
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	void putdata(Value m, Value x) override;
	Value getdata(Value m) override;
	Value rangeTo(int i, int j) override;
	Value rangeLen(int i, int n) override;

	SuObject* ob_if_ob() override
		{ return this; }

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

	virtual bool erase(Value m);
	virtual bool erase2(Value m);

	bool operator==(const SuObject& ob) const;
	bool eq(const SuValue& y) const override;
	bool lt(const SuValue& y) const override;
	int order() const override;
	size_t hashfn() const override;
	size_t hashcontrib() const override;

	size_t packsize() const override;
	void pack(char* buf) const override;
	static SuObject* unpack(const gcstring& s);
	static SuObject* unpack2(const gcstring& s, SuObject* ob);

	SuObject* slice(size_t offset);

	void set_readonly()
		{ readonly = true; }
	bool get_readonly() const
		{ return readonly; }

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
	Value unique();
private:
	void append(Value x);
	void insert(int i, Value x);
	void migrateMapToVec();
	static Mfn method(Value member);
	void ck_readonly() const;
	Value getdefault(Value member, Value def);
	Value Size(BuiltinArgs& args);
	Value Members(BuiltinArgs& args);
	Value Iter(BuiltinArgs& args);
	Value MemberQ(BuiltinArgs& args);
	Value MethodQ(BuiltinArgs& args);
	Value MethodClass(BuiltinArgs& args);
	Value Base(BuiltinArgs& args);
	Value BaseQ(BuiltinArgs& args);
	Value Eval(BuiltinArgs& args);
	Value Eval2(BuiltinArgs& args);
	Value Set_default(BuiltinArgs& args);
	Value Copy(BuiltinArgs& args);
	Value Partition(BuiltinArgs& args);
	Value Sort(BuiltinArgs& args);
	Value LowerBound(BuiltinArgs& args);
	Value UpperBound(BuiltinArgs& args);
	Value EqualRange(BuiltinArgs& args);
	Value Unique(BuiltinArgs& args);
	Value Slice(BuiltinArgs& args);
	Value Find(BuiltinArgs& args);
	Value Erase(BuiltinArgs& args);
	Value Delete(BuiltinArgs& args);
	void clear();
	Value Add(BuiltinArgs& args);
	Value Reverse(BuiltinArgs& args);
	Value Join(BuiltinArgs& args);
	Value Set_readonly(BuiltinArgs& args);
	void setReadonly();
	Value ReadonlyQ(BuiltinArgs& args);
	Value Values(BuiltinArgs& args);
	Value Assocs(BuiltinArgs& args);
	Value GetDefault(BuiltinArgs& args);

	Vector vec;
	Map map;
	bool readonly = false;
	int version = 0; // incremented when member is added or removed
	// used to detect modification during iteration
	friend class ModificationCheck;
	friend class MemBase;
	};

Value su_object();
