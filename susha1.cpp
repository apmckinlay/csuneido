/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2002 Suneido Software Corp.
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

#include "gsl-lite.h"
#include <windows.h>
#include <wincrypt.h>
#include "builtinclass.h"
#include "gcstring.h"
#include "sufinalize.h"
#include "sustring.h"
#include "except.h"

class Sha1 : public SuFinalize
	{
public:
	Sha1()
		{
		if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0))
			if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
				except("Sha1: CryptAcquireContext failed");
		if (! CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hHash))
			except("Sha1: CryptCreateHash failed");
		}

	void out(Ostream& os) const override
		{ os << "Sha1()"; }
	static auto methods()
		{
		static Method<Sha1> methods[]
			{
			{ "Update", &Update },
			{ "Value", &ValueFn }
			};
		return gsl::make_span(methods);
		}
	const char* type() const override
		{ return "Sha1"; }
	void update(gcstring gcstr) const;
	gcstring value() const;
	void finalize() override;

private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);

	HCRYPTPROV hCryptProv;
	HCRYPTHASH hHash;
	};

Value su_sha1()
	{
	static BuiltinClass<Sha1> Sha1Class("(@strings)");
	return &Sha1Class;
	}

template<>
void BuiltinClass<Sha1>::out(Ostream& os) const
	{ os << "Sha1 /* builtin class */"; }

template<>
Value BuiltinClass<Sha1>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: new Sha1()");
	args.end();
	return new BuiltinInstance<Sha1>();
	}

template<>
Value BuiltinClass<Sha1>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: Sha1(@strings)");
	Sha1* a = new BuiltinInstance<Sha1>();
	if (! args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return new SuString(a->value());
	}

Value Sha1::Update(BuiltinArgs& args)
	{
	args.usage("usage: sha1.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
	}

void Sha1::update(gcstring s) const
	{
	if (! CryptHashData(hHash, (BYTE*) s.ptr(), s.size(), 0))
		except("Sha1: CryptHashData failed");
	}

const int SHA1_SIZE = 20;

gcstring Sha1::value() const
	{
	DWORD dwHashLen = SHA1_SIZE;
	gcstring out(dwHashLen);
	if (! CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*) out.ptr(), &dwHashLen, 0))
		except("Sha1: CryptGetHashParam failed");
	verify(dwHashLen == SHA1_SIZE);
	return out;
	}

Value Sha1::ValueFn(BuiltinArgs& args)
	{
	args.usage("usage: sha1.Value()");
	args.end();
	return new SuString(value());
	}

void Sha1::finalize()
	{
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	}

// for auth
gcstring sha1(const gcstring& s)
	{
	Sha1* a = new BuiltinInstance<Sha1>();
	a->update(s);
	return a->value();
	}
