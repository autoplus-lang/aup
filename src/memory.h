#ifndef _AUP_MEMORY_H
#define _AUP_MEMORY_H
#pragma once

#include "util.h"

typedef struct {
	size_t size;
	size_t : sizeof(size_t);	// padding
} aupM;

/*
	memory block: [-info-header-|-returned-pointer-]
	//
	aupM *mem = malloc(sizeof(aupM) + size);
	mem->size = size;
	void *ptr = (uintptr_t)mem + sizeof(aupM);
*/

#define AUP_ALLOC(type, count) \
	(type *)aup_realloc(NULL, 0, sizeof(type) * (count))

#define AUP_FREE(type, ptr) \
	aup_realloc(ptr, sizeof(type), 0)

#define AUP_GROW_CAP(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define AUP_GROW_ARR(type, ptr, oldCount, count) \
	(type *)aup_realloc(ptr, sizeof(type) * (oldCount), sizeof(type) * (count))

#define AUP_FREE_ARR(type, ptr, oldCount) \
	aup_realloc(ptr, sizeof(type) * (oldCount), 0)

void *aup_realloc(void *previous, size_t oldSize, size_t newSize);
void aup_freeObjects(AUP_VM);

#endif
