#ifndef _AUP_VALUE_H
#define _AUP_VALUE_H
#pragma once

#include "util.h"

typedef enum {
	// General value types
	AUP_TNIL  = 0,
    AUP_TOBJ  = 1,
	AUP_TBOOL = 2,
	AUP_TINT  = 4,
	AUP_TNUM  = 6,
	// Object types
	AUP_TSTR,
	AUP_TFUN,
	AUP_TUPV
} aupVt;

typedef enum {
	AUP_TNIL_NIL	= AUP_CMB(AUP_TNIL, AUP_TNIL),
	AUP_TNIL_BOOL	= AUP_CMB(AUP_TNIL, AUP_TBOOL),
	AUP_TNIL_INT	= AUP_CMB(AUP_TNIL, AUP_TINT),
	AUP_TNIL_NUM	= AUP_CMB(AUP_TNIL, AUP_TNUM),
	AUP_TNIL_OBJ	= AUP_CMB(AUP_TNIL, AUP_TOBJ),

	AUP_TBOOL_NIL	= AUP_CMB(AUP_TBOOL, AUP_TNIL),
	AUP_TBOOL_BOOL	= AUP_CMB(AUP_TBOOL, AUP_TBOOL),
	AUP_TBOOL_INT	= AUP_CMB(AUP_TBOOL, AUP_TINT),
	AUP_TBOOL_NUM	= AUP_CMB(AUP_TBOOL, AUP_TNUM),
	AUP_TBOOL_OBJ	= AUP_CMB(AUP_TBOOL, AUP_TOBJ),

	AUP_TINT_NIL	= AUP_CMB(AUP_TINT, AUP_TNIL),
	AUP_TINT_BOOL	= AUP_CMB(AUP_TINT, AUP_TBOOL),
	AUP_TINT_INT	= AUP_CMB(AUP_TINT, AUP_TINT),
	AUP_TINT_NUM	= AUP_CMB(AUP_TINT, AUP_TNUM),
	AUP_TINT_OBJ	= AUP_CMB(AUP_TINT, AUP_TOBJ),

	AUP_TNUM_NIL	= AUP_CMB(AUP_TNUM, AUP_TNIL),
	AUP_TNUM_BOOL	= AUP_CMB(AUP_TNUM, AUP_TBOOL),
	AUP_TNUM_INT	= AUP_CMB(AUP_TNUM, AUP_TINT),
	AUP_TNUM_NUM	= AUP_CMB(AUP_TNUM, AUP_TNUM),
	AUP_TNUM_OBJ	= AUP_CMB(AUP_TNUM, AUP_TOBJ),

	AUP_TOBJ_NIL	= AUP_CMB(AUP_TOBJ, AUP_TNIL),
	AUP_TOBJ_BOOL	= AUP_CMB(AUP_TOBJ, AUP_TBOOL),
	AUP_TOBJ_INT	= AUP_CMB(AUP_TOBJ, AUP_TINT),
	AUP_TOBJ_NUM	= AUP_CMB(AUP_TOBJ, AUP_TNUM),
	AUP_TOBJ_OBJ	= AUP_CMB(AUP_TOBJ, AUP_TOBJ)
} aupVtt;

typedef struct _aupObj aupObj;
typedef struct _aupOstr aupOstr;
typedef struct _aupOfun aupOfun;
typedef struct _aupOupv aupOupv;

typedef struct {
    union {
        aupVt type;
        unsigned notNil : 4;
        unsigned isObj  : 1;
    };
	union {
		bool Bool : 1;
		int64_t Int;
		double Num;
        aupObj *Obj;
		uint64_t _;
	};
} aupVal;

typedef struct {
	int count;
	int capacity;
    aupVal *values;
} aupValArr;

#define AUP_NIL         ((aupVal){ .type = AUP_TNIL })
#define AUP_FALSE       ((aupVal){ .type = AUP_TBOOL, .Bool = 0 })
#define AUP_TRUE        ((aupVal){ .type = AUP_TBOOL, .Bool = 1 })
#define AUP_BOOL(b)     ((aupVal){ .type = AUP_TBOOL, .Bool = (b) })
#define AUP_INT(i)      ((aupVal){ .type = AUP_TINT, .Int = (i) })
#define AUP_NUM(n)      ((aupVal){ .type = AUP_TNUM, .Num = (n) })
#define AUP_OBJ(o)      ((aupVal){ .isObj = true, .Obj = (aupObj *)(o) })

#define AUP_IS_NIL(v)   (!(v).notNil)
#define AUP_IS_BOOL(v)  ((v).type == AUP_TBOOL)
#define AUP_IS_INT(v)   ((v).type == AUP_TINT)
#define AUP_IS_NUM(v)   ((v).type == AUP_TNUM)
#define AUP_IS_OBJ(v)   ((v).isObj)

#define AUP_AS_BOOL(v)  ((v).Bool)
#define AUP_AS_INT(v)   ((v).Int)
#define AUP_AS_NUM(v)   ((v).Num)
#define AUP_AS_OBJ(v)   ((v).Obj)
#define AUP_AS_RAW(v)	((v)._)

#define AUP_TYPE(v)     ((v).type)
#define AUP_IS_FALSEY(v) (!(bool)((v)._))

void aup_initValueArr(aupValArr *array);
void aup_freeValueArr(aupValArr *array);
int aup_writeValueArr(aupValArr *array, aupVal value);
int aup_findValue(aupValArr *array, aupVal value);

const char *aup_typeofVal(aupVal value);
void aup_printVal(aupVal value);

#endif
