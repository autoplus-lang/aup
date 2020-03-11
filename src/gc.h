#ifndef _AUP_GC_H
#define _AUP_GC_H
#pragma once

#include "util.h"
#include "object.h"

struct _aupGC {
    size_t nextGC;
    size_t allocated;
    aupObj *objects;
    aupObj **grayStack;
    int    grayCount;
    int    graySpace;
    aupTab strings;
    aupTab globals;
};

#define AUP_PushRoot(vm, obj) \
    ((vm)->tempRoots[(vm)->numRoots++] = (obj))
#define AUP_PopRoot(vm) \
    ((vm)->numRoots--)

void aup_initGC(aupGC *gc);
void aup_freeGC(aupGC *gc);

void aup_collect(aupVM *vm);
void *aup_realloc(aupVM *vm, aupGC *gc, void *ptr, size_t old, size_t _new);

#endif
