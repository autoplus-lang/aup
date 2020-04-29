#ifndef _AUP_GC_H
#define _AUP_GC_H
#pragma once

#include "util.h"
#include "object.h"

#define AUP_PushRoot(vm, obj) \
    ((vm)->tempRoots[(vm)->numRoots++] = (obj))
#define AUP_PopRoot(vm) \
    ((vm)->numRoots--)

void aup_initGC();
void aup_freeGC();

aupTab *aup_getStrings();
aupTab *aup_getGlobals();

void *aup_alloc(size_t size);
void *aup_realloc(void *ptr, size_t old, size_t _new);
void aup_dealloc(void *ptr, size_t);
void *aup_allocObject(size_t size, aupTObj type);

void aup_collect();

#endif
