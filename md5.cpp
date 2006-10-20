/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2002 Suneido Software Corp. 
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

// must define this to get Crypt functions
#define _WIN32_WINNT	0x0400

#include <windows.h>
#include <wincrypt.h>

bool md5(char* data, int datalen, char* buf)
	// pre: buf must point to 16 bytes of memory
	{
	HCRYPTPROV hCryptProv;
	if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0))
		if (! CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
			return false;

	HCRYPTHASH hHash;
	if (! CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash))
		return false;
	
	if (! CryptHashData(hHash, (BYTE*) data, datalen, 0)) 
		return false;
	
	DWORD dwHashLen = 16;
	if (! CryptGetHashParam(hHash, HP_HASHVAL, (unsigned char*) buf, &dwHashLen, 0))
		return false;
	
	CryptDestroyHash(hHash);
	CryptReleaseContext(hCryptProv, 0);
	return true;
	}
