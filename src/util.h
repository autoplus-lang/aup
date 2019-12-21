#ifndef _AUP_UTIL_H
#define _AUP_UTIL_H

#include "aup.h"

typedef enum {
	AUP_OK = 0,
	AUP_COMPILE_ERR,
	AUP_RUNTIME_ERR
} aupStt;

typedef struct _aupVM aupVM;

#define AUP_VM	aupVM *vm

#endif