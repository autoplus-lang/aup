#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "compiler.h"
#include "table.h"

#define AUP_MAX_FRAMES	(64)
#define AUP_MAX_STACK	(AUP_MAX_LOCALS + AUP_MAX_ARGS) * AUP_MAX_FRAMES

typedef struct {
	aupOf *function;
	uint32_t *ip;
	aupV *stack;
} aupCf;

struct _aupVM {
    aupCtx *ctx;

	aupCf frames[AUP_MAX_FRAMES];
	int frameCount;

	aupV *top;
	aupV stack[AUP_MAX_STACK];

	aupOu *openUpvalues;
	aupO *objects;
};

aupVM *aupVM_new(aupCtx *ctx);
void aupVM_free(aupVM *vm);
int aupVM_interpret(aupVM *vm, const char *source);

#endif
