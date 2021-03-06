// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <wincrypt.h>
#include "builtinclass.h"
#include "gcstring.h"
#include "sustring.h"
#include <span>

int Md5_count = 0;

class Md5 : public SuValue {
public:
	Md5() {
		if (!CryptAcquireContext(
				&hCryptProv, nullptr, nullptr, PROV_RSA_FULL, 0))
			if (!CryptAcquireContext(&hCryptProv, nullptr, nullptr,
					PROV_RSA_FULL, CRYPT_NEWKEYSET))
				except("Md5: CryptAcquireContext failed");
		if (!CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash))
			except("Md5: CryptCreateHash failed");
		++Md5_count;
	}
	static auto methods() {
		static Method<Md5> methods[]{
			{"Update", &Md5::Update}, {"Value", &Md5::ValueFn}};
		return std::span(methods);
	}
	void update(gcstring gcstr);
	gcstring value();
	void close();

private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);

	HCRYPTPROV hCryptProv{};
	HCRYPTHASH hHash{};
};

Value su_md5() {
	static BuiltinClass<Md5> Md5Class("(@strings)");
	return &Md5Class;
}

template <>
Value BuiltinClass<Md5>::instantiate(BuiltinArgs& args) {
	args.usage("new Md5()").end();
	return new BuiltinInstance<Md5>();
}

template <>
Value BuiltinClass<Md5>::callclass(BuiltinArgs& args) {
	args.usage("Md5(@strings)");
	Md5* a = new BuiltinInstance<Md5>();
	if (!args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return new SuString(a->value());
}

Value Md5::Update(BuiltinArgs& args) {
	args.usage("md5.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
}

void Md5::update(gcstring s) {
	if (!CryptHashData(hHash, (BYTE*) s.ptr(), s.size(), 0))
		except("Md5: CryptHashData failed");
}

const int MD5_SIZE = 16;

gcstring Md5::value() {
	DWORD dwHashLen = MD5_SIZE;
	gcstring out(dwHashLen);
	if (!CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*) out.ptr(), &dwHashLen, 0))
		except("Md5: CryptGetHashParam failed");
	verify(dwHashLen == MD5_SIZE);
	close();
	return out;
}

Value Md5::ValueFn(BuiltinArgs& args) {
	args.usage("md5.Value()").end();
	return new SuString(value());
}

void Md5::close() {
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	--Md5_count;
}
