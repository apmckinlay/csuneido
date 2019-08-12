// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// NOTE: you can't use incremental linking with this code

#include "callback.h"
#include "interp.h"
#include "except.h"
#include "exceptimp.h"
#include "hashmap.h"
#include "sublock.h"
#include "suobject.h"
#include "builtin.h"
#include "heap.h"
#include "list.h"
#include "suinstance.h"

int callback(Value fn, Callback* cb, char* src);

static Heap heap(true); // true = executable

#if defined(X_MSC_VER)
__declspec(naked) int master() {
	_asm
	{
		lea	EAX,4[ESP]
		push EAX // src
		push 0x55555555 // cb
		push 0x66666666 // fn
		mov EAX, offset callback
		call EAX // ensure call uses absolute address (not relative)
		add ESP,12
		ret 0x4444
	}
}
#else
uint8_t master[32] = {
	0x8D, 0x44, 0x24, 0x04,       // lea eax,[esp+4]
	0x50,                         // push eax
	0x68, 0x55, 0x55, 0x55, 0x55, // push 55555555h
	0x68, 0x66, 0x66, 0x66, 0x66, // push 66666666h
	0xB8, 0x77, 0x77, 0x77, 0x77, // mov eax, 77777777h
	0xFF, 0xD0,                   // call eax
	0x83, 0xC4, 0x0C,             // add esp,0Ch
	0xC2, 0x44, 0x44              // ret 4444h
};
const int CALL_OFFSET = 16;
#endif

const int LENGTH = 32;
const int CB_OFFSET = 6;
const int FN_OFFSET = 11;
const int ARGS_OFFSET = 26;
/*
  Returns a callable pointer
  The actual code is defined above in master()
  which is copied into a new memory block then
  the appropriate arguments are added to the new block
  and the pointer is returned.
*/
void* make_callback(Value fn, Callback* cb, short nargs) {
	verify(nargs < 256);
	char* p = (char*) heap.alloc(LENGTH);
	memset(p, 0, LENGTH);
	memcpy(p, (const void*) &master, LENGTH);
	verify(*((int*) (p + CB_OFFSET)) == 0x55555555);
	verify(*((int*) (p + FN_OFFSET)) == 0x66666666);
	verify(*((short*) (p + ARGS_OFFSET)) == 0x4444);
	*((Value*) (p + FN_OFFSET)) = fn;
	*((Callback**) (p + CB_OFFSET)) = cb;
	verify(*((int*) (p + CALL_OFFSET)) == 0x77777777);
	*((void**) (p + CALL_OFFSET)) = (void*) &callback;
	*((short*) (p + ARGS_OFFSET)) = nargs;
	return p;
}

struct Cb {
	Cb() {
	}
	Cb(void* f, Callback* c) : fn(f), cb(c) {
	}
	void* fn = nullptr;
	Callback* cb = nullptr;
};
struct CbHashFn {
	size_t operator()(Value x) {
		if (val_cast<SuInstance*>(x))
			return reinterpret_cast<size_t>(x.ptr());
		return x.hash();
	}
};
struct CbEq {
	bool operator()(Value x, Value y) {
		if (val_cast<SuInstance*>(x) && val_cast<SuInstance*>(y))
			return x.ptr() == y.ptr();
		return x == y;
	}
};
typedef HashMap<Value, Cb, CbHashFn, CbEq> CbHash;
CbHash callbacks;

void Callback::put(char*& dst, char*& dst2, const char* lim2, Value x) {
	void* f;
	int n;
	if (!x)
		f = nullptr;
	else if (x.int_if_num(&n))
		f = (void*) n;
	else if (Cb* cb = callbacks.find(x)) {
		if (cb->cb != this)
			except("duplicate callback with different type");
		f = cb->fn;
	} else {
		f = make_callback(x, this, TypeMulti::size());
		callbacks[x] = Cb(f, this);
	}
	*((void**) dst) = f;
	dst += sizeof(void*);
}

static List<void*> to_free1;
static List<void*> to_free2;

int cbtofree_count = 0;

// called by timer
void free_callbacks() {
	// alternate between two to_free lists so there is a guaranteed delay
	to_free1.swap(to_free2);
	if (to_free1.empty())
		return;
	for (auto x : to_free1)
		heap.free(x);
	cbtofree_count -= to_free1.size();
	to_free1.clear();
}

const int TF_LIMIT = 10000;

BUILTIN(ClearCallback, "(value)") {
	Value x = TOP();
	Cb* cb = callbacks.find(x);
	if (cb) {
		if (to_free1.size() > TF_LIMIT) {
			free_callbacks();
			errlog("ClearCallback to_free > limit, timer not working?");
		}
		to_free1.add(cb->fn);
		++cbtofree_count;
	}
	callbacks.erase(x);
	return cb ? SuTrue : SuFalse;
}

// called by the functions created by make_callback
int callback(Value fn, Callback* cb, char* src) {
	return cb->callback(fn, src);
}

extern void handler(const Except&);

int Callback::callback(Value fn, const char* src) {
	KEEPSP
	try {
		try {
			// convert & push args
			for (int i = 0; i < nitems; ++i) {
				Value x = items[i].type().get(src, Value());
				PUSH(x ? (Value) x : (Value) &SuNumber::zero);
			}

			// call function
			return toInt(docall(fn, CALL, nitems));
		} catch (const std::exception& e) {
			except(e.what());
		}
#ifndef _MSC_VER
		catch (const Except&) {
			throw;
		}
		// with Visual C++, this catches page faults ???
		catch (...) {
			except("unknown exception");
		}
#endif
	} catch (const Except& e) {
		handler(e);
		return 0;
	}
}

void Callback::out(Ostream& os) const {
	os << named.name() << " /* callback */";
	/*
	for (int i = 0; i < nitems; ++i) {
		if (i > 0)
			os << ", ";
		items[i].out(os);
		os << ' ' << names(mems[i]);
	}
	os << ')';
	*/
}

BUILTIN(Callbacks, "()") {
	SuObject* ob = new SuObject;
	for (auto& callback : callbacks)
		ob->add(callback.key);
	return ob;
}
