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

void *aup_defaultAlloc(void *ptr, size_t size);

void *aup_realloc(AUP_VM, void *previous, size_t oldSize, size_t newSize);
void aup_freeObjects(AUP_VM);

void aup_gc(AUP_VM);
void aup_markValue(AUP_VM, aupVal value);
void aup_markObject(AUP_VM, aupObj *object);

#endif
