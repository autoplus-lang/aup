
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stddef.h>
#include <stdint.h>

// stdbool.h
#if !(defined(_STDBOOL) || defined(STDBOOL_H) || defined(_STDBOOL_H) || defined(__STDBOOL) || defined(__STDBOOL_H))
#define __bool_true_false_are_defined	1
#ifndef __cplusplus
#if _MSC_VER <= 1600
#define bool    char
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

/*
typedef enum {
    AUP_TNIL,
    AUP_TBOOL,
    AUP_TNUM,
    AUP_TOBJ
} aupT;
*/