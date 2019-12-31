#ifndef _AUP_UTIL_H
#define _AUP_UTIL_H

#include "aup.h"

enum {
    AUP_INVALID       = -1,
	AUP_OK            =  0,
	AUP_COMPILE_ERROR =  1,
	AUP_RUNTIME_ERROR =  2
};

typedef void * (*aupAlloc)(void *ptr, size_t size);
typedef struct _aupVM aupVM;

#define AUP_VM	aupVM  *vm

// Combine two low bytes
#define AUP_CMB(l, r)	(uint8_t)(((char)(l) & 0xF) | ((char)(r) & 0xF) << 4)
#define AUP_CMB_L(c)	(char)(((c) & 0xF))
#define AUP_CMB_R(c)	(char)((((c) >> 4) & 0xF))

char *aup_readFile(const char *path, size_t *size);

#endif
