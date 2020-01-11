#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "common.h"
#include "object.h"
#include "table.h"

#define AUP_MAX_FRAMES  64
#define AUP_MAX_STACK   (AUP_MAX_FRAMES * UINT8_COUNT)

typedef struct {
    uint8_t *ip;
    aupVal *slots;
    aupFun *function;
} aupFrame;

struct _aupVM {
    aupVal *top;
    aupVal stack[AUP_MAX_STACK];
    aupFrame frames[AUP_MAX_FRAMES];
    int frameCount;

    int numRoots;
    aupObj *tempRoots[8];
    aupUpv *openUpvalues;

    aupCompiler *compiler;
    aupVM *next;

    aupGC *gc;
    aupTab *strings;
    aupTab *globals;

    char *errmsg;
    bool hadError;
};

aupVM *aup_create();
void aup_close(aupVM *vm);
aupVM *aup_cloneVM(aupVM *from);
int aup_doFile(aupVM *vm, const char *fname);

aupVal aup_error(aupVM *vm, const char *msg, ...);

void aup_push(aupVM *vm, aupVal value);
void aup_pop(aupVM *vm);
void aup_pushRoot(aupVM *vm, aupObj *object);
void aup_popRoot(aupVM *vm);

void aup_defineNative(aupVM *vm, const char *name, aupCFn function);
void aup_setGlobal(aupVM *vm, const char *name, aupVal value);
aupVal aup_getGlobal(aupVM *vm, const char *name);

void aup_loadMath(aupVM *vm);

#endif
