#include "win.h"
#include <iphlpapi.h>
#include "gcstring.h"

// return mac address of FIRST adapter
gcstring get_mac_address()
	{
	IP_ADAPTER_INFO info[10];
	
	ULONG buflen = sizeof (info);
	if (NO_ERROR != GetAdaptersInfo(info, &buflen))
		return "";
	return gcstring((char*) info[0].Address, info[0].AddressLength);
	}

#include "prim.h"
#include "sustring.h"

Value su_get_mac_address()
	{
	return new SuString(get_mac_address());
	}
PRIM(su_get_mac_address, "GetMacAddress()");
