/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2006 Suneido Software Corp. 
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
