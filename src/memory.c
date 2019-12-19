#include <stdlib.h>

#include "util.h"
#include "memory.h"

void *aup_realloc(void *ptr, size_t oldSize, size_t newSize)
{
	if (newSize == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newSize);
}