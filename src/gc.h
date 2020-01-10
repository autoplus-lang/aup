#ifndef _AUP_GC_H
#define _AUP_GC_H
#pragma once

#include "common.h"
#include "object.h"

struct _aupGC {
    size_t nextGC;
    size_t allocated;
    aupObj *objects;
};

void aup_initGC(aupGC *gc);
void aup_freeGC(aupGC *gc);

void *aup_realloc(aupGC *gc, void *ptr, size_t old, size_t new);
void aup_collect(aupVM *vm);

#endif
