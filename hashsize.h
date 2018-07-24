#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

/* primes */
const size_t hash_size[] = {0, 5, 11, 19, 31, 61, 127, 251, 509, 761, 1021,
	1531, 2039, 3001, 4093, 6007, 8191, 12289, 16381, 32749, 65521, 131071,
	262139, 524287, 1048573};
const int n_hash_sizes = sizeof hash_size / sizeof(size_t);
