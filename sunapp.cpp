// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// suneido asynchronous protocol handler for web browser component

#include "sunapp.h"
#include "alert.h"
#include "call.h"
#include <windows.h>
#include <wininet.h> // for decoding url

//#define LOGGING

#ifdef LOGGING
#include "ostreamfile.h"
static OstreamFile log("sunapp.log");
#define LOG(stuff) (log << (void*) this << ": " << stuff << endl), log.flush()
#include "gcstring.h"
#else
#define LOG(stuff)
#endif

class CSuneidoAPP : public IInternetProtocol {
public:
	CSuneidoAPP() {
		LOG("construct");
	}

	virtual ~CSuneidoAPP() {
		LOG("destruct");
		LocalFree((void*) str);
	}

	// bypass garbage collection!
	void* operator new(size_t n) {
		return LocalAlloc(LPTR, n);
	}
	void operator delete(void* p) {
		LocalFree(p);
	}

	// IUnknown
	virtual HRESULT __stdcall QueryInterface(const IID& iid, void** ppv);
	virtual ULONG __stdcall AddRef();
	virtual ULONG __stdcall Release();

	// IInternetProtocolRoot

	HRESULT STDMETHODCALLTYPE Start(
		/* [in] */ LPCWSTR szUrl,
		/* [in] */ IInternetProtocolSink __RPC_FAR* pOIProtSink,
		/* [in] */ IInternetBindInfo __RPC_FAR* pOIBindInfo,
		/* [in] */ DWORD grfPI,
		/* [in] */ DWORD dwReserved);

	HRESULT STDMETHODCALLTYPE Continue(
		/* [in] */ PROTOCOLDATA __RPC_FAR* pProtocolData) {
		LOG("Continue");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Abort(
		/* [in] */ HRESULT hrReason,
		/* [in] */ DWORD dwOptions) {
		LOG("Abort");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Terminate(
		/* [in] */ DWORD dwOptions) {
		LOG("Terminate");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Suspend(void) {
		return E_NOTIMPL;
	} // Not implemented

	HRESULT STDMETHODCALLTYPE Resume(void) {
		return E_NOTIMPL;
	} // Not implemented

	// IInternetProtocol based on IInternetProtocolRoot

	HRESULT STDMETHODCALLTYPE Read(
		/* [length_is][size_is][out][in] */ void __RPC_FAR* pv,
		/* [in] */ ULONG cb,
		/* [out] */ ULONG __RPC_FAR* pcbRead);

	HRESULT STDMETHODCALLTYPE Seek(
		/* [in] */ LARGE_INTEGER dlibMove,
		/* [in] */ DWORD dwOrigin,
		/* [out] */ ULARGE_INTEGER __RPC_FAR* plibNewPosition) {
		LOG("Seek");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE LockRequest(
		/* [in] */ DWORD dwOptions) {
		LOG("LockRequest");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE UnlockRequest(void) {
		LOG("UnlockRequest");
		return S_OK;
	}

private:
	ULONG len = 0;
	ULONG pos = 0;
	const char* str = nullptr;
	long m_cRef = 1; // Reference count
};

// IUnknown implementation

HRESULT __stdcall CSuneidoAPP::QueryInterface(const IID& iid, void** ppv) {
	if (iid == IID_IUnknown || iid == IID_IInternetProtocol) {
		*ppv = static_cast<IInternetProtocol*>(this);
	} else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

ULONG __stdcall CSuneidoAPP::AddRef() {
	LOG("AddRef");
	return InterlockedIncrement(&m_cRef);
}

ULONG __stdcall CSuneidoAPP::Release() {
	LOG("Release " << m_cRef);
	if (InterlockedDecrement(&m_cRef) == 0) {
		delete this;
		return 0;
	}
	return m_cRef;
}

// IInternetProtocol

char* localmemdup(const char* s, int n) {
	return (char*) memcpy(LocalAlloc(LPTR, n), s, n);
}

#define USES_CONVERSION \
	int _convert; \
	_convert; \
	LPCWSTR _lpw; \
	_lpw;
inline LPSTR WINAPI AtlW2AHelper(LPSTR lpa, LPCWSTR lpw, int nChars) {
	lpa[0] = '\0';
	WideCharToMultiByte(CP_ACP, 0, lpw, -1, lpa, nChars, NULL, NULL);
	return lpa;
}
#define W2CA(w) \
	((LPCSTR)(((_lpw = (w)) == NULL) \
			? NULL \
			: (_convert = (lstrlenW(_lpw) + 1) * 2, \
				  AtlW2AHelper((LPSTR) _alloca(_convert), _lpw, _convert))))

HRESULT STDMETHODCALLTYPE CSuneidoAPP::Start(
	/* [in] */ LPCWSTR szUrl,
	/* [in] */ IInternetProtocolSink __RPC_FAR* pOIProtSink,
	/* [in] */ IInternetBindInfo __RPC_FAR* pOIBindInfo,
	/* [in] */ DWORD grfPI,
	/* [in] */ DWORD dwReserved) {
	USES_CONVERSION;
	const char* url = W2CA(szUrl);
	ULONG buflen = strlen(url) + 10; // shouldn't get any bigger?
	char* buf = new char[buflen];
	InternetCanonicalizeUrl(url, buf, &buflen, ICU_DECODE | ICU_NO_ENCODE);
	LOG("Start: " << buf);

	int n;
	str = trycall("SuneidoAPP", buf, &n);
	if (str == 0)
		return INET_E_DATA_NOT_AVAILABLE;
	str = localmemdup(str, n);
	len = n;
	pos = 0;

	LOG("ReportData");
	pOIProtSink->ReportData(BSCF_DATAFULLYAVAILABLE | BSCF_LASTDATANOTIFICATION,
		len, len); // MUST call this
	LOG("end Start");
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSuneidoAPP::Read(
	/* [length_is][size_is][out][in] */ void __RPC_FAR* pv,
	/* [in] */ ULONG cb,
	/* [out] */ ULONG __RPC_FAR* pcbRead) {
	*pcbRead = min(cb, len - pos);
	LOG("Read cb=" << cb << " len=" << len << " pos=" << pos
				   << " *pcbRead=" << *pcbRead);
	memcpy(pv, str + pos, *pcbRead);
	pos += *pcbRead;
	if (pos < len)
		return S_OK;
	LOG("FALSE");
	return S_FALSE;
}

// Class factory ====================================================

class CFactory : public IClassFactory {
public:
	// IUnknown
	virtual HRESULT __stdcall QueryInterface(const IID& iid, void** ppv);
	virtual ULONG __stdcall AddRef();
	virtual ULONG __stdcall Release();

	// Interface IClassFactory
	virtual HRESULT __stdcall CreateInstance(
		IUnknown* pUnknownOuter, const IID& iid, void** ppv);
	virtual HRESULT __stdcall LockServer(BOOL bLock);

	virtual ~CFactory() = default;

private:
	long m_cRef = 1;
};

//
// Class factory IUnknown implementation
//
HRESULT __stdcall CFactory::QueryInterface(const IID& iid, void** ppv) {
	if ((iid == IID_IUnknown) || (iid == IID_IClassFactory)) {
		*ppv = static_cast<IClassFactory*>(this);
	} else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	reinterpret_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

ULONG __stdcall CFactory::AddRef() {
	return InterlockedIncrement(&m_cRef);
}

ULONG __stdcall CFactory::Release() {
	if (InterlockedDecrement(&m_cRef) == 0) {
		delete this;
		return 0;
	}
	return m_cRef;
}

// IClassFactory implementation
HRESULT __stdcall CFactory::CreateInstance(
	IUnknown* pUnknownOuter, const IID& iid, void** ppv) {
	// Cannot aggregate.
	if (pUnknownOuter != NULL) {
		return CLASS_E_NOAGGREGATION;
	}

	// Create component.
	CSuneidoAPP* pA = new CSuneidoAPP;
	if (pA == NULL) {
		return E_OUTOFMEMORY;
	}

	// Get the requested interface.
	HRESULT hr = pA->QueryInterface(iid, ppv);

	// Release the IUnknown pointer.
	// (If QueryInterface failed, component will delete itself.)
	pA->Release();
	return hr;
}

// LockServer
HRESULT __stdcall CFactory::LockServer(BOOL bLock) {
	return S_OK;
}

// register & revoke ================================================

const CLSID CLSID_SuneidoAPP = {0xBFBE2090, 0x6BBA, 0x11D4,
	{0xBC, 0x13, 0x00, 0x60, 0x6E, 0x30, 0xB2, 0x58}};
const char* clsid = "{BFBE2090-6BBA-11D4-BC13-00606E30B258}";

static CFactory factory;

void sunapp_register_classes() {
	HRESULT hr;
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
		alert("CoInitialize failed");

	IInternetSession* iis;
	hr = CoInternetGetSession(0, &iis, 0);
	if (FAILED(hr))
		alert("CoInternetGetSession failed");
	else {
		iis->RegisterNameSpace(&factory, CLSID_SuneidoAPP, L"suneido", 0, 0, 0);
		iis->Release();
	}
}

void sunapp_revoke_classes() {
	CoUninitialize();
}
