#ifndef _AUP_MEMORY_H
#define _AUP_MEMORY_H
#pragma once

#include "util.h"

#define AUP_ALLOC(type, count) \
	(type *)aup_realloc(vm, NULL, 0, sizeof(type) * (count))

#define AUP_FREE(type, ptr) \
	aup_realloc(vm, ptr, sizeof(type), 0)

#define AUP_GROW_CAP(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define AUP_GROW_ARR(type, ptr, oldCount, count) \
	(type *)aup_realloc(vm, ptr, sizeof(type) * (oldCount), sizeof(type) * (count))

#define AUP_FREE_ARR(type, ptr, oldCount) \
	aup_realloc(vm, ptr, sizeof(type) * (oldCount), 0)

void *aup_realloc(AUP_VM, void *previous, size_t oldSize, size_t newSize);
void aup_freeObjects(AUP_VM);

void aup_gc(AUP_VM);
void aup_markValue(AUP_VM, aupV value);
void aup_markObject(AUP_VM, aupO *object);

#endif
