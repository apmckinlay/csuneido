#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <ole2.h>

extern "C" __declspec(dllexport) int __stdcall traccel(
	void* p, int message, int wParam) {
	auto iunk = static_cast<IUnknown*>(p);
	IOleInPlaceActiveObject* pi;
	HRESULT hr = iunk->QueryInterface(
		IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&pi));
	if (!SUCCEEDED(hr) || !pi)
		return false;
	MSG msg;
	msg.message = message;
	msg.wParam = wParam;
	hr = pi->TranslateAccelerator(&msg);
	pi->Release();
	return SUCCEEDED(hr);
}

BOOL APIENTRY DllMain(
	HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}
