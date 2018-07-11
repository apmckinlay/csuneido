// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "builtinclass.h"
#include "suboolean.h"
#include "gcstring.h"
#include "checksum.h"

class SuAdler32 : public SuValue
	{
public:
	SuAdler32() : value(1)
		{ }
	static auto methods()
		{
		static Method<SuAdler32> methods[]
			{
			{ "Update", &SuAdler32::Update },
			{ "Value", &SuAdler32::ValueFn }
			};
		return gsl::make_span(methods);
		}
	void update(const gcstring& s);
	uint32_t value;
private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);
	};

Value su_adler32()
	{
	static BuiltinClass<SuAdler32> suAdler32Class("(@strings)");
	return &suAdler32Class;
	}

template<>
Value BuiltinClass<SuAdler32>::instantiate(BuiltinArgs& args)
	{
	args.usage("new Adler32()").end();
	return new BuiltinInstance<SuAdler32>();
	}

template<>
Value BuiltinClass<SuAdler32>::callclass(BuiltinArgs& args)
	{
	args.usage("Adler32(@strings)");
	SuAdler32* a = new BuiltinInstance<SuAdler32>();
	if (! args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return a->value;
	}

Value SuAdler32::Update(BuiltinArgs& args)
	{
	args.usage("adler32.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
	}

void SuAdler32::update(const gcstring& s)
	{
	value = checksum(value, s.ptr(), s.size());
	}

Value SuAdler32::ValueFn(BuiltinArgs& args)
	{
	args.usage("adler32.Value()").end();
	return value;
	}
