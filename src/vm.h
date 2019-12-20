#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "compiler.h"

#define AUP_MAX_FRAMES	(1)
#define AUP_MAX_STACK	(AUP_MAX_LOCALS + AUP_MAX_ARGS) * AUP_MAX_FRAMES

typedef struct _aupVM {
	uint32_t *ip;
	aupCh *chunk;
	aupV *top;
	aupV stack[AUP_MAX_STACK];
} aupVM;

enum {
	AUP_OK = 0,
	AUP_COMPILE_ERR,
	AUP_RUNTIME_ERR
};

#endif
