#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "compiler.h"
#include "table.h"

#define AUP_MAX_FRAMES	(1)
#define AUP_MAX_STACK	(AUP_MAX_LOCALS + AUP_MAX_ARGS) * AUP_MAX_FRAMES

struct _aupVM {
	uint32_t *ip;
	aupCh *chunk;
	aupV *top;
	aupV stack[AUP_MAX_STACK];

	aupT strings;
	aupT globals;

	aupO *objects;
};

aupVM *aupVM_new();
void aupVM_free(aupVM *vm);

#endif
