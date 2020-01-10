#ifndef _AUP_COMMON_H
#define _AUP_COMMON_H
#pragma once

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stddef.h>
#include <stdint.h>

// stdbool.h
#if !(defined(_STDBOOL) || defined(STDBOOL_H) || defined(_STDBOOL_H) || defined(__STDBOOL) || defined(__STDBOOL_H))
#define __bool_true_false_are_defined	1
#ifndef __cplusplus
#if defined(_MSC_VER) && _MSC_VER <= 1600
#define bool    unsigned char
#else
#define bool    _Bool
#endif
#define false   0
#define true    1
#endif
#endif

// architecture
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_x64) || defined(__x86_x64__) || defined(__amd64) || defined(__amd64__)
#define AUP_X64    1
#elif defined(_WIN32) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__i386) || defined(__i386__) || defined(__I86__)
#define AUP_X86    1
#else
#error "This architecture is not supported!"
#endif

// debug mode
#if (defined(DEBUG) || defined(_DEBUG)) && !(defined(NDEBUG) || defined(_NDEBUG))
#define AUP_DEBUG
#endif

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

#endif
