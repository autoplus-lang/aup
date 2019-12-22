#ifndef _AUP_VALUE_H
#define _AUP_VALUE_H
#pragma once

#include "util.h"

typedef enum {
	AUP_TNIL,
	AUP_TBOOL,
	AUP_TNUM,
	AUP_TOBJ,

	AUP_TSTR,

} aupVt;

typedef struct _aupO aupO;
typedef struct _aupOs aupOs;

typedef uint64_t aupV;

typedef struct {
	int count;
	int capacity;
	aupV *values;
} aupVa;

typedef union {
	uint64_t b;
	uint32_t bs[2];
	double f;
} aupVn;

#define AUP_SBIT        ((uint64_t)1 << 63)
#define AUP_QNAN        ((uint64_t)0x7ffc000000000000ULL)

#define AUP_TAG_NIL     1
#define AUP_TAG_FALSE   2
#define AUP_TAG_TRUE    3

#define AUP_NIL         ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_NIL))
#define AUP_FALSE       ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_FALSE))
#define AUP_TRUE        ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_TRUE))
#define AUP_BOOL(b)     ((b) ? AUP_TRUE : AUP_FALSE)
#define AUP_NUM(n)      ((aupVn){ .f =(n) }).b
#define AUP_OBJ(o)      (aupV)(AUP_SBIT | AUP_QNAN | (uint64_t)(uintptr_t)(o))

#define AUP_IS_NIL(v)   ((v) == AUP_NIL)
#define AUP_IS_BOOL(v)  (((v) & (AUP_QNAN | AUP_TAG_FALSE)) == (AUP_QNAN | AUP_TAG_FALSE))
#define AUP_IS_NUM(v)   (((v) & AUP_QNAN) != AUP_QNAN)
#define AUP_IS_OBJ(v)   (((v) & (AUP_QNAN | AUP_SBIT)) == (AUP_QNAN | AUP_SBIT))

#define AUP_AS_BOOL(v)  ((v) == AUP_TRUE)
#define AUP_AS_NUM(v)   ((aupVn){ .b=(v) }).f
#define AUP_AS_OBJ(v)   ((aupO *)(uintptr_t)((v) & ~(AUP_SBIT | AUP_QNAN)))

#define AUP_IS_FALSE(v) (!(value) || AUP_IS_NIL(value) \
	|| (AUP_IS_BOOL(value) && AUP_AS_BOOL(value) == false) \
	|| (AUP_IS_OBJ(value) && AUP_AS_OBJ(value) == NULL))

void aupVa_init(aupVa *array);
void aupVa_free(aupVa *array);
int aupVa_write(aupVa *array, aupV value);

const char *aupV_typeOf(aupV value);
void aupV_print(aupV value);

#endif
