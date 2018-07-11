// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "interp.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sustring.h"
#include "date.h"
#include "sudate.h"
#include "ostream.h"
#include <objbase.h>

inline LPWSTR WINAPI AtlA2WHelper(LPWSTR lpw, LPCSTR lpa, int nChars)
	{
	lpw[0] = '\0';
	MultiByteToWideChar(CP_ACP, 0, lpa, -1, lpw, nChars);
	return lpw;
	}
#define USES_CONVERSION \
	int _convert; (void) _convert;\
	const char* _lpa; (void) _lpa;
#define A2OLE(s) \
	(((_lpa = s) == NULL) ? NULL : ( _convert = (strlen(_lpa)+1), AtlA2WHelper((LPWSTR) _alloca(_convert*2), _lpa, _convert)))

// a wrapper for COM objects
// that allows access via IDispatch
class SuCOMobject : public  SuValue
	{
public:
	explicit SuCOMobject(IDispatch* id, const char* pi = "???") 
		: idisp(id), progid(pi), isdisp(true)
		{ verify(NULL != idisp); }

	explicit SuCOMobject(IUnknown* iu, const char* pi = "???") 
		: iunk(iu), progid(pi), isdisp(false)
		{ verify(NULL != iunk); }
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	// properties
	Value getdata(Value) override;
	void putdata(Value, Value) override;
	IDispatch* idispatch() const
		{ return isdisp ? idisp : NULL; }
	IUnknown * iunknown() const
		{ return isdisp ? NULL : iunk; }
private:
	void verify_not_released() const;
	void require_idispatch() const;
	union
		{
		IUnknown* iunk;
		IDispatch* idisp;
		void* isalive;
		};
	const char* progid; // never NULL, but might be '???' if progid isn't known
	bool isdisp; // indicates whether this is an IDispatch or just an IUnknown
	};

void SuCOMobject::verify_not_released() const
	{
	if (! isalive)
		except("COM: " << progid << " already released");
	}

void SuCOMobject::require_idispatch() const
	{
	if (! isdisp)
		except("COM: " << progid << " doesn't support IDispatch");
	}

void SuCOMobject::out(Ostream& os) const
	{
	os << "COMobject('" << progid << "')";
	}

static void check_result(HRESULT hr, const char* progid, const char* name, const char* action)
	{
	if (hr == DISP_E_BADPARAMCOUNT)
		except("COM: " << progid << " " << action << " " << name << " bad param count");
	else if (hr == DISP_E_BADVARTYPE)
		except("COM: " << progid << " " << action << " " << name << " bad var type");
	else if (hr == DISP_E_EXCEPTION)
		except("COM: " << progid << " " << action << " " << name << " exception");
	else if (hr == DISP_E_MEMBERNOTFOUND)
		except("COM: " << progid << " " << action << " " << name << " member not found");
	else if (hr == DISP_E_NONAMEDARGS)
		except("COM: " << progid << " " << action << " " << name << " no named args");
	else if (hr == DISP_E_OVERFLOW)
		except("COM: " << progid << " " << action << " " << name << " overflow");
	else if (hr == DISP_E_PARAMNOTFOUND)
		except("COM: " << progid << " " << action << " " << name << " param not found");
	else if (hr == DISP_E_TYPEMISMATCH)
		except("COM: " << progid << " " << action << " " << name << " type mismatch");
	else if (hr == DISP_E_PARAMNOTOPTIONAL)
		except("COM: " << progid << " " << action << " " << name << " param not optional");
	else if (FAILED(hr))
		except("COM: " << progid << " " << action << " " << name << " failed");
	}

static Value com2su(VARIANT* var)
	{
	HRESULT hr;
	Value result = 0;

	/* get a fully dereferenced copy of the variant */
	/* ### we may want to optimize this sometime... avoid copying values */
	VARIANT varValue;
	VariantInit(&varValue);
	VariantCopyInd(&varValue, var);
	VariantClear(var);

	USES_CONVERSION;
	switch (V_VT(&varValue))
		{
	case VT_BOOL:
		result = V_BOOL(&varValue) == 0 ? SuFalse : SuTrue;
		break ;
	case VT_I2:
		result = V_I2(&varValue);
		break;
	case VT_I4:
		result = V_I4(&varValue);
		break;
	case VT_UI2:
		result = V_UI2(&varValue);
		break;
	case VT_UI4:
		result = V_UI4(&varValue);
		break;
	case VT_R4:
		result = SuNumber::from_float(V_R4(&varValue));
		break ;
	case VT_R8:
		result = SuNumber::from_double(V_R8(&varValue));
		break ;
	case VT_DISPATCH:
		{
		IDispatch* idisp = V_DISPATCH(&varValue);
		if (idisp)
			{
			idisp->AddRef(); // VariantClear will release the reference from varValue
			result = new SuCOMobject(idisp);
			}
		}
		break;
	case VT_UNKNOWN:
		{
		IUnknown* iunk = V_UNKNOWN(&varValue);
		IDispatch* idisp = 0;
		hr = iunk->QueryInterface(IID_IDispatch, (void**) &idisp);
		if (SUCCEEDED(hr) && idisp)
			result = new SuCOMobject(idisp);
		else
			{
			iunk->AddRef(); // VariantClear() will release the reference from varValue
			result = new SuCOMobject(iunk);
			}
		}
		break;
	case VT_NULL:
	case VT_EMPTY:
		result = 0;
		break;
	case VT_BSTR:
		{ // VariantClear() will release the BSTR in varValue
		int nw = SysStringLen(V_BSTR(&varValue));
		if (nw == 0)
			return SuEmptyString;
		int n = WideCharToMultiByte(CP_ACP, 0, V_BSTR(&varValue), nw, 0, 0, NULL, NULL);
		char* s = (char*) _alloca(n);
		n = WideCharToMultiByte(CP_ACP, 0, V_BSTR(&varValue), nw, s, n, NULL, NULL);
		if (n == 0)
			except("COM: string conversion error");
		result = new SuString(s, n);
		break;
		}
	case VT_DATE:
		{
		DATE wdt = varValue.date;
		SYSTEMTIME sdt;
		BOOL ret;

		ret = VariantTimeToSystemTime(wdt, &sdt);
		if (ret == FALSE)
			except("COM: date conversion error");
		DateTime dat = DateTime(
			sdt.wYear,
			sdt.wMonth,
			sdt.wDay,
			sdt.wHour,
			sdt.wMinute,
			sdt.wSecond,
			sdt.wMilliseconds);
		result = new SuDate(dat.date(), dat.time());
		break;
		}
	default:
		except("COM: can't convert to Suneido value");
		}

	VariantClear(&varValue);
	return result;
	}

Value SuCOMobject::getdata(Value member)
	{
	verify_not_released();
	require_idispatch();
	// get id from name
	auto name = member.str();
	USES_CONVERSION;
	OLECHAR* wname = A2OLE(name);
	DISPID dispid;
	HRESULT hr = idisp->GetIDsOfNames(IID_NULL, &wname, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
	if (FAILED(hr))
		except("COM: " << progid << " doesn't have " << name);
	// convert args
	DISPPARAMS args = { NULL, NULL, 0, 0 };
	// invoke
	VARIANT result;
	hr = idisp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET,
		&args, &result, NULL, NULL);
	check_result(hr, progid, name, "get");
	// convert return value
	return com2su(&result);
	}

inline BSTR A2WBSTR(LPCSTR lp, int nLen = -1)
	{
	int n = MultiByteToWideChar(CP_ACP, 0, lp, nLen, NULL, 0) - 1;
	BSTR s = ::SysAllocStringLen(NULL, n);
	if (s != NULL)
		MultiByteToWideChar(CP_ACP, 0, lp, -1, s, n);
	return s;
	}

static void su2com(Value x, VARIANT* v)
	{
	int n;
	const char* s;
	if (x == SuTrue || x == SuFalse)
		{
		V_VT(v) = VT_BOOL;
		V_BOOL(v) = (x == SuTrue ? VARIANT_TRUE : VARIANT_FALSE);
		}
	else if (x.int_if_num(&n))
		{
		V_VT(v) = VT_I4;
		V_I4(v) = n;
		}
	else if (SuNumber* num = val_cast<SuNumber*>(x))
		{
		V_VT(v) = VT_R8;
		V_R8(v) = num->to_double();
		}
	else if (nullptr != (s = x.str_if_str()))
		{
		V_VT(v) = VT_BSTR;
		V_BSTR(v) = A2WBSTR(s); // COM convention is callee will free memory
		// TODO: handle strings with embedded nuls
		}
	else if (SuCOMobject* sco = val_cast<SuCOMobject*>(x))
		{
		IDispatch * idisp = sco->idispatch();
		if (idisp)
			{
			V_VT(v) = VT_DISPATCH;
			V_DISPATCH(v) = idisp;
			idisp->AddRef(); // COM convention is callee will Release()
			}
		else
			{
			IUnknown * iunk = sco->iunknown();
			verify(NULL != iunk);
			V_VT(v) = VT_UNKNOWN;
			V_UNKNOWN(v) = iunk;
			iunk->AddRef(); // COM convention is callee will Release()
			}
		}
	else
		except("COM: can't convert: " << x);
	}

void SuCOMobject::putdata(Value member, Value val)
	{
	verify_not_released();
	require_idispatch();
	// get id from name
	auto name = member.str();
	USES_CONVERSION;
	OLECHAR* wname = A2OLE(name);
	DISPID dispid;
	HRESULT hr = idisp->GetIDsOfNames(IID_NULL, &wname, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
	if (FAILED(hr))
		except("COM: " << progid << " doesn't have " << name);
	// convert args
	VARIANTARG arg;
	su2com(val, &arg);
    // Property puts have named arg that represents the value being assigned to the property.
    DISPID put = DISPID_PROPERTYPUT;
	DISPPARAMS args = { &arg, &put, 1, 1 };
	// invoke
	hr = idisp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYPUT,
		&args, NULL, NULL, NULL);
	check_result(hr, progid, name, "put");
	}

Value SuCOMobject::call(Value self, Value member, 
	short nargs, short nargnames, short* argnames, int each)
	{
	verify_not_released();
	static Value RELEASE("Release");
	static Value DISPATCHQ("Dispatch?");
	if (member == RELEASE)
		{
		NOARGS("comobject.Release()");
		isdisp ? idisp->Release() : iunk->Release();
		isalive = nullptr;
		return 0;
		}
	else if (member == DISPATCHQ)
		{
		NOARGS("comobject.Dispatch?()");
		return isdisp ? SuTrue : SuFalse;
		}
	// else call

	// get id from name
	auto name = member.str();
	USES_CONVERSION;
	OLECHAR* wname = A2OLE(name);
	DISPID dispid;
	HRESULT hr = idisp->GetIDsOfNames(IID_NULL, &wname, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
	if (FAILED(hr))
		except("COM: " << progid << " doesn't have " << name);

	// convert args
	DISPPARAMS args = { NULL, NULL, 0, 0 };
	args.cArgs = nargs;
	VARIANT* vargs = (VARIANT*) _alloca(nargs * sizeof (VARIANT));
	for (int i = 0; i < nargs; ++i)
		su2com(ARG(i), &vargs[nargs - i - 1]);
	args.rgvarg = vargs;

	VARIANT result;
	VariantInit(&result);
	hr = idisp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD,
		&args, &result, NULL, NULL);
	check_result(hr, progid, name, "call");

	return com2su(&result);
	}

// su_COMobject() ===================================================

#include "builtin.h"

BUILTIN(COMobject, "(progid)")
	{
	const int nargs = 1;
	HRESULT hr;
	auto progid = "???";
	IDispatch* idisp = nullptr;
	IUnknown* iunk = nullptr;
	int n;
	if (ARG(0).int_if_num(&n) && nullptr != (iunk = (IUnknown*)n))
		{
		hr = iunk->QueryInterface(IID_IDispatch, (void**) &idisp);
		if (SUCCEEDED(hr) && idisp)
			{
			iunk->Release();
			return new SuCOMobject(idisp, progid);
			}
		else
			return new SuCOMobject(iunk, progid);
		}
	else
		{
		progid = ARG(0).str();
		// get clsid from progid
		CLSID clsid;
		USES_CONVERSION;
		hr = CLSIDFromProgID(A2OLE(progid), &clsid);
		if (FAILED(hr))
			return SuFalse;
		// get idispatch
		hr = CoCreateInstance(clsid, NULL, CLSCTX_SERVER, IID_IDispatch, (void**) &idisp);
		if (SUCCEEDED(hr) && idisp)
			return new SuCOMobject(idisp, progid);
		hr = CoCreateInstance(clsid, NULL, CLSCTX_SERVER, IID_IUnknown, (void **) &iunk);
		if (SUCCEEDED(hr) && iunk)
			return new SuCOMobject(iunk, progid);
		}
	return SuFalse;
	}
