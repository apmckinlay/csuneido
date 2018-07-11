// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <wincrypt.h>
#include "builtinclass.h"
#include "gcstring.h"
#include "sustring.h"
#include "except.h"

int Sha1_count = 0;

class Sha1 : public SuValue
	{
public:
	Sha1()
		{
		if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0))
			if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
				except("Sha1: CryptAcquireContext failed");
		if (! CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hHash))
			except("Sha1: CryptCreateHash failed");
		++Sha1_count;
		}

	static auto methods()
		{
		static Method<Sha1> methods[]
			{
			{ "Update", &Sha1::Update },
			{ "Value", &Sha1::ValueFn }
			};
		return gsl::make_span(methods);
		}
	void update(gcstring gcstr);
	gcstring value();

private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);
	void close();

	HCRYPTPROV hCryptProv;
	HCRYPTHASH hHash;
	};

Value su_sha1()
	{
	static BuiltinClass<Sha1> Sha1Class("(@strings)");
	return &Sha1Class;
	}

template<>
Value BuiltinClass<Sha1>::instantiate(BuiltinArgs& args)
	{
	args.usage("new Sha1()").end();
	return new BuiltinInstance<Sha1>();
	}

template<>
Value BuiltinClass<Sha1>::callclass(BuiltinArgs& args)
	{
	args.usage("Sha1(@strings)");
	Sha1* a = new BuiltinInstance<Sha1>();
	if (! args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return new SuString(a->value());
	}

Value Sha1::Update(BuiltinArgs& args)
	{
	args.usage("sha1.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
	}

void Sha1::update(gcstring s)
	{
	if (! CryptHashData(hHash, (BYTE*) s.ptr(), s.size(), 0))
		except("Sha1: CryptHashData failed");
	}

const int SHA1_SIZE = 20;

gcstring Sha1::value()
	{
	DWORD dwHashLen = SHA1_SIZE;
	gcstring out(dwHashLen);
	if (! CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*) out.ptr(), &dwHashLen, 0))
		except("Sha1: CryptGetHashParam failed");
	verify(dwHashLen == SHA1_SIZE);
	close();
	return out;
	}

Value Sha1::ValueFn(BuiltinArgs& args)
	{
	args.usage("sha1.Value()").end();
	return new SuString(value());
	}

void Sha1::close()
	{
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	--Sha1_count;
	}

// for auth
gcstring sha1(const gcstring& s)
	{
	Sha1* a = new BuiltinInstance<Sha1>();
	a->update(s);
	return a->value();
	}
