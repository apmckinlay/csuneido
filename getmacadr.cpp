// Copyright (c) 2006 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <iphlpapi.h>
#include "sustring.h"
#include "suobject.h"

// return mac address of FIRST adapter
gcstring get_mac_address()
	{
	IP_ADAPTER_INFO info[10];

	ULONG buflen = sizeof (info);
	if (NO_ERROR != GetAdaptersInfo(info, &buflen))
		return "";
	for (int i = 0; i < 10; ++i)
		if (info[i].AddressLength > 0)
			return gcstring((char*) info[i].Address, info[i].AddressLength);
	return "";
	}

// return a list of mac addresses
SuObject* get_mac_addresses()
	{
	IP_ADAPTER_INFO tmp;
	IP_ADAPTER_INFO* info = &tmp;

	ULONG buflen = sizeof (tmp);
	if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(info, &buflen))
		info = (IP_ADAPTER_INFO*) _alloca(buflen);
	SuObject* list = new SuObject();
	if (NO_ERROR != GetAdaptersInfo(info, &buflen))
		return list;
	for (; info; info = info->Next)
		if (info->AddressLength > 0)
			list->add(new SuString(gcstring((char*) info->Address, info->AddressLength)));
	return list;
	}

#include "builtin.h"

BUILTIN(GetMacAddress, "()")
	{
	return new SuString(get_mac_address());
	}

BUILTIN(GetMacAddresses, "()")
	{
	return get_mac_addresses();
	}
