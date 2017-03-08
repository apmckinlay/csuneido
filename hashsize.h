#ifndef HASHSIZE_H
#define HASHSIZE_H

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

/* primes */
const size_t hash_size[] =
	{ 0, 5, 11, 19, 31, 61, 127, 251, 509, 761, 1021, 1531, 2039,
	3001, 4093, 6007, 8191, 12289, 16381, 32749, 65521, 131071,
	262139, 524287, 1048573 };
const int n_hash_sizes = sizeof hash_size / sizeof (size_t);

#endif
