/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
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

#include "array.h"
#include "testing.h"

TEST(array)
	{
	Array<int, 11> a;

	for (int i = 1; i < 10; i += 2)
		a.append(i);

	verify(a.size() == 5);

	a.insert(6);
	a.insert(10);
	a.insert(2);
	a.insert(4);
	a.insert(0);
	a.insert(8);

	verify(a.size() == 11);
	verify(a.capacity() == 11);
	a.erase(4);
	a.erase(5);
	verify(a.size() == 9);
	}
