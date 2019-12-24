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
	AUP_TFUN

} aupVt;

typedef struct _aupO aupO;
typedef struct _aupOs aupOs;
typedef struct _aupOf aupOf;

typedef struct {
	aupVt type;
	union {
		bool Bool : 1;
		double Num;
		aupO *Obj;
		uint64_t _;
	};
} aupV;

typedef struct {
	int count;
	int capacity;
	aupV *values;
} aupVa;

#define AUP_NIL         ((aupV){ .type = AUP_TNIL })
#define AUP_FALSE       ((aupV){ .type = AUP_TBOOL, .Bool = 0 })
#define AUP_TRUE        ((aupV){ .type = AUP_TBOOL, .Bool = 1 })
#define AUP_BOOL(b)     ((aupV){ .type = AUP_TBOOL, .Bool = (b) })
#define AUP_NUM(n)      ((aupV){ .type = AUP_TNUM, .Num = (n) })
#define AUP_OBJ(o)      ((aupV){ .type = AUP_TOBJ, .Obj = (aupO *)(o) })

#define AUP_IS_NIL(v)   ((v).type == AUP_TNIL)
#define AUP_IS_BOOL(v)  ((v).type == AUP_TBOOL)
#define AUP_IS_NUM(v)   ((v).type == AUP_TNUM)
#define AUP_IS_OBJ(v)   ((v).type == AUP_TOBJ)

#define AUP_AS_BOOL(v)  ((v).Bool)
#define AUP_AS_NUM(v)   ((v).Num)
#define AUP_AS_OBJ(v)   ((v).Obj)
#define AUP_AS_RAW(v)	((v)._)

#define AUP_VAL_TYPE(v) ((v).type)
#define AUP_IS_FALSEY(v) (!(bool)((v)._))

void aupVa_init(aupVa *array);
void aupVa_free(aupVa *array);
int aupVa_write(aupVa *array, aupV value);
int aupVa_find(aupVa *array, aupV value);

const char *aupV_typeOf(aupV value);
void aupV_print(aupV value);

#endif
