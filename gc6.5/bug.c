#include "include/gc.h"
#include <stdio.h>
#include <memory.h>

#define SIZE 4104
#define N 1000
#define FILL 0x15
#define OFFSET 1100

int main(int argc, char** argv)
	{
	char* blocks[N];
	int i, j;
	
	for (i = 0; i < N; ++i)
		{
		blocks[i] = (char*) GC_malloc_atomic(SIZE);
		memset(blocks[i], FILL, SIZE);
		}
	
	GC_gcollect();
	for (i = 0; i < N; ++i)
		GC_malloc(i * 5);
	GC_gcollect();
	
	for (i = 0; i < N; ++i)
		for (j = 0; j < SIZE; ++j)
			if (blocks[i][j] != FILL)
				{
				printf("phase 1 - blocks[%d][%d] modified\n", i, j);
				return 1;
				}
			
	for (i = 0; i < N; ++i)
		blocks[i] += OFFSET;
	
	GC_gcollect();
	for (i = 0; i < N; ++i)
		GC_malloc(i * 5);
	GC_gcollect();
	
	for (i = 0; i < N; ++i)
		for (j = 0; j < SIZE; ++j)
			if (blocks[i][j - OFFSET] != FILL)
				{
				printf("phase 2 - blocks[%d][%d] modified\n", i, j);
				return 1;
				}

	return 0;
	}
