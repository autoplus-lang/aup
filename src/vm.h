#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "compiler.h"
#include "table.h"

#define AUP_MAX_FRAMES	    (64)
#define AUP_MAX_STACK	    (AUP_MAX_LOCALS + AUP_MAX_ARGS) * AUP_MAX_FRAMES

#define AUP_MAX_TEMP_ROOTS  5

typedef struct {
	aupOf *function;
	uint32_t *ip;
	aupV *stack;
} aupCallFrame;

struct _aupVM {
    aupV *top;
    aupV stack[AUP_MAX_STACK];

    aupCallFrame frames[AUP_MAX_FRAMES];
	int frameCount;

    aupT strings;
    aupT globals;
    aupAlloc alloc;

	aupOu *openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;

    aupObj *objects;
    int grayCount;
    int grayCapacity;
    aupObj **grayStack;

    aupObj *tempRoots[AUP_MAX_TEMP_ROOTS];
    int numTempRoots;
};

aupVM *aup_create(aupAlloc allocator);
void aup_close(aupVM *vm);
int aup_doString(aupVM *vm, const char *source);
int aup_doFile(aupVM *vm, const char *name);

void aup_pushRoot(aupVM *vm, aupObj *object);
void aup_popRoot(aupVM *vm);

#endif
