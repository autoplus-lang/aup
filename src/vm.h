#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "code.h"
#include "value.h"
#include "gc.h"

#define AUP_MAX_FRAMES  64
#define AUP_MAX_STACK   (AUP_MAX_FRAMES * UINT8_COUNT)

typedef struct {
    uint32_t *ip;
    aupVal *stack;
    aupFun *function;
} aupFrame;

struct _aupVM {
    aupVal *top;
    aupVal stack[AUP_MAX_STACK];
    aupFrame frames[AUP_MAX_FRAMES];
    int frameCount;

    int numRoots;
    aupObj *tempRoots[8];
    aupUpv *openUpvals;

    aupVM *next;
};

aupVM *aup_createVM(aupVM *from);
void aup_closeVM(aupVM *vm);
int aup_interpret(aupVM *vm, aupSrc *source);

#endif
