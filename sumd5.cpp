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

#include <windows.h>
#include <wincrypt.h>
#include "builtinclass.h"
#include "suboolean.h"
#include "gcstring.h"
#include "sufinalize.h"
#include "sustring.h"

class Md5 : public SuFinalize
	{
public:
	Md5()
		{
		if (! CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_FULL, 0))
			if (! CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_NEWKEYSET))
				except("Md5: CryptAcquireContext failed");
		if (! CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash))
			except("Md5: CryptCreateHash failed");
		}
	void out(Ostream& os) const override
		{ os << "Md5()"; }
	static Method<Md5>* methods()
		{
		static Method<Md5> methods[] =
			{
			Method<Md5>("Update", &Md5::Update),
			Method<Md5>("Value", &Md5::ValueFn),
			Method<Md5>("", 0)
			};
		return methods;
		}
	const char* type() const override
		{ return "Md5"; }
	void update(gcstring gcstr);
	gcstring value();
	void finalize() override;

private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);

	HCRYPTPROV hCryptProv;
	HCRYPTHASH hHash;
	};

Value su_md5()
	{
	static BuiltinClass<Md5> Md5Class("(@strings)");
	return &Md5Class;
	}

template<>
void BuiltinClass<Md5>::out(Ostream& os) const
	{ os << "Md5 /* builtin class */"; }

template<>
Value BuiltinClass<Md5>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: new Md5()");
	args.end();
	return new BuiltinInstance<Md5>();
	}

template<>
Value BuiltinClass<Md5>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: Md5(@strings)");
	Md5* a = new BuiltinInstance<Md5>();
	if (! args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return new SuString(a->value());
	}

Value Md5::Update(BuiltinArgs& args)
	{
	args.usage("usage: md5.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
	}

void Md5::update(gcstring s)
	{
	if (! CryptHashData(hHash, (BYTE*) s.ptr(), s.size(), 0))
		except("Md5: CryptHashData failed");
	}

const int MD5_SIZE = 16;

gcstring Md5::value()
	{
	DWORD dwHashLen = MD5_SIZE;
	gcstring out(dwHashLen);
	if (! CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*) out.ptr(), &dwHashLen, 0))
		except("Md5: CryptGetHashParam failed");
	verify(dwHashLen == MD5_SIZE);
	return out;
	}

Value Md5::ValueFn(BuiltinArgs& args)
	{
	args.usage("usage: md5.Value()");
	args.end();
	return new SuString(value());
	}

void Md5::finalize()
	{
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	}
