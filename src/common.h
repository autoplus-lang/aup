#pragma once

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef UINT8_COUNT
#define UINT8_COUNT (UINT8_MAX + 1)
#endif

#define AUP_COMBINE(l, r)   (uint8_t)(((char)(l) & 0xF) | ((char)(r) & 0xF) << 4)
#define AUP_CLEFT(c)        (char)(((c) & 0xF))
#define AUP_CRIGHT(c)	    (char)((((c) >> 4) & 0xF))

#define AUP_GROWCAP(c)  (((c) < 8) ? 8 : ((c) * 2))

enum {
    AUP_OK = 0,
    AUP_COMPILE_ERROR,
    AUP_RUNTIME_ERROR,
    AUP_INIT_ERROR = -1,
};

typedef struct _aupVM aupVM;
typedef struct _aupGC aupGC;
typedef struct _aupVal aupVal;

uint32_t aup_hashBytes(const void *bytes, size_t size);
char *aup_readFile(const char *path, size_t *size);
