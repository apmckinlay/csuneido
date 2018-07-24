// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <wincrypt.h>
#include "builtinclass.h"
#include "gcstring.h"
#include "sustring.h"
#include "except.h"

// TODO refactor duplication with Sha1

int Sha256_count = 0;

class Sha256 : public SuValue {
public:
	Sha256() {
		if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, 0))
			if (!CryptAcquireContext(
					&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_NEWKEYSET))
				except("Sha256: CryptAcquireContext failed");
		if (!CryptCreateHash(hCryptProv, CALG_SHA_256, 0, 0, &hHash))
			except("Sha256: CryptCreateHash failed");
		++Sha256_count;
	}

	static auto methods() {
		static Method<Sha256> methods[]{
			{"Update", &Sha256::Update}, {"Value", &Sha256::ValueFn}};
		return gsl::make_span(methods);
	}
	void update(gcstring gcstr);
	gcstring value();

private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);
	void close();

	HCRYPTPROV hCryptProv{};
	HCRYPTHASH hHash{};
};

Value su_sha256() {
	static BuiltinClass<Sha256> Sha256Class("(@strings)");
	return &Sha256Class;
}

template <>
Value BuiltinClass<Sha256>::instantiate(BuiltinArgs& args) {
	args.usage("new Sha256()").end();
	return new BuiltinInstance<Sha256>();
}

template <>
Value BuiltinClass<Sha256>::callclass(BuiltinArgs& args) {
	args.usage("Sha256(@strings)");
	Sha256* a = new BuiltinInstance<Sha256>();
	if (!args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return new SuString(a->value());
}

Value Sha256::Update(BuiltinArgs& args) {
	args.usage("sha256.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
}

void Sha256::update(gcstring s) {
	if (!CryptHashData(hHash, (BYTE*) s.ptr(), s.size(), 0))
		except("Sha256: CryptHashData failed");
}

const int SHA256_SIZE = 32;

gcstring Sha256::value() {
	DWORD dwHashLen = SHA256_SIZE;
	gcstring out(dwHashLen);
	if (!CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*) out.ptr(), &dwHashLen, 0))
		except("Sha256: CryptGetHashParam failed");
	verify(dwHashLen == SHA256_SIZE);
	close();
	return out;
}

Value Sha256::ValueFn(BuiltinArgs& args) {
	args.usage("sha256.Value()").end();
	return new SuString(value());
}

void Sha256::close() {
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	--Sha256_count;
}
