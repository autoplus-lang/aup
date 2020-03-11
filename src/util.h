#ifndef _AUP_UTIL_H
#define _AUP_UTIL_H
#pragma once

#include "aup.h"

#ifndef THREAD_LOCAL
#if (__STDC_VERSION__ >= 201112) && !defined(__STDC_NO_THREADS__)
#define THREAD_LOCAL    _Thread_local
#elif defined(_MSC_VER)
#define THREAD_LOCAL    __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL    __thread
#endif
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#endif

#ifndef UINT8_COUNT
#define UINT8_COUNT     (UINT8_MAX + 1)
#endif
#ifndef UINT16_COUNT
#define UINT16_COUNT    (UINT16_MAX + 1)
#endif

#define AUP_PAIR(l, r)  (uint8_t)(((char)(l)) | ((char)(r)) << 4)
#define AUP_GROW(cap)   (((cap) < 8) ? 8 : ((cap) << 1))

// N, bytes1, length1, ..., bytesN, lengthN
uint32_t aup_hashBytes(int count, ...);
char *aup_readFile(const char *path, size_t *size);

typedef struct _aupVM aupVM;
typedef struct _aupGC aupGC;
typedef struct _aupVal aupVal;

typedef aupVal(*aupCFn)(aupVM *vm, int argc, aupVal *args);

enum {
    AUP_OK,
    AUP_COMPILE_ERROR,
    AUP_RUNTIME_ERROR
};

#endif
