// Copyright (c) 2006 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <iphlpapi.h>
#include "sustring.h"
#include "suobject.h"
#include "builtin.h"

BUILTIN(GetMacAddresses, "()") {
	IP_ADAPTER_INFO tmp;
	IP_ADAPTER_INFO* info = &tmp;

	ULONG buflen = sizeof(tmp);
	if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(info, &buflen))
		info = (IP_ADAPTER_INFO*) _alloca(buflen);
	SuObject* list = new SuObject();
	if (NO_ERROR != GetAdaptersInfo(info, &buflen))
		return list;
	for (; info; info = info->Next)
		if (info->AddressLength > 0)
			list->add(new SuString(
				gcstring((char*) info->Address, info->AddressLength)));
	return list;
}
