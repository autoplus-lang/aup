#ifndef _AUP_VALUE_H
#define _AUP_VALUE_H
#pragma once

#include "common.h"

typedef struct _aupObj aupObj;
typedef struct _aupStr aupStr;
typedef struct _aupFun aupFun;
typedef struct _aupUpv aupUpv;
typedef struct _aupMap aupMap;

typedef aupVal (* aupCFn)(aupVM *vm, int argc, aupVal *args);

typedef enum {
    AUP_TNIL,
    AUP_TBOOL,
    AUP_TNUM,
    AUP_TPTR,
    AUP_TCFN,
    AUP_TOBJ,
} aupVType;

typedef enum {
    AUP_TSTR,
    AUP_TFUN,
    AUP_TUPV,
    AUP_TMAP,
} aupOType;

enum {
    AUP_TNIL_NIL    = AUP_COMBINE(AUP_TNIL, AUP_TNIL),
    AUP_TNIL_BOOL   = AUP_COMBINE(AUP_TNIL, AUP_TBOOL),
    AUP_TNIL_NUM    = AUP_COMBINE(AUP_TNIL, AUP_TNUM),
    AUP_TNIL_OBJ    = AUP_COMBINE(AUP_TNIL, AUP_TOBJ),

    AUP_TBOOL_NIL   = AUP_COMBINE(AUP_TBOOL, AUP_TNIL),
    AUP_TBOOL_BOOL  = AUP_COMBINE(AUP_TBOOL, AUP_TBOOL),
    AUP_TBOOL_NUM   = AUP_COMBINE(AUP_TBOOL, AUP_TNUM),
    AUP_TBOOL_OBJ   = AUP_COMBINE(AUP_TBOOL, AUP_TOBJ),

    AUP_TNUM_NIL    = AUP_COMBINE(AUP_TNUM, AUP_TNIL),
    AUP_TNUM_BOOL   = AUP_COMBINE(AUP_TNUM, AUP_TBOOL),
    AUP_TNUM_NUM    = AUP_COMBINE(AUP_TNUM, AUP_TNUM),
    AUP_TNUM_OBJ    = AUP_COMBINE(AUP_TNUM, AUP_TOBJ),

    AUP_TOBJ_NIL    = AUP_COMBINE(AUP_TOBJ, AUP_TNIL),
    AUP_TOBJ_BOOL   = AUP_COMBINE(AUP_TOBJ, AUP_TBOOL),
    AUP_TOBJ_NUM    = AUP_COMBINE(AUP_TOBJ, AUP_TNUM),
    AUP_TOBJ_OBJ    = AUP_COMBINE(AUP_TOBJ, AUP_TOBJ),

    AUP_TCFN_CFN    = AUP_COMBINE(AUP_TCFN, AUP_TCFN),
    AUP_TPTR_PTR    = AUP_COMBINE(AUP_TPTR, AUP_TPTR)
};

struct _aupVal {
    aupVType type;
    union {
        bool Bool;
        double Num;
        aupObj *Obj;
        aupCFn CFn;
        void *Ptr;
        uint64_t Raw;
    };
};

typedef struct {
    int count;
    int capacity;
    aupVal *values;
} aupArr;

static const aupVal AUP_NIL = { AUP_TNIL };
static const aupVal AUP_TRUE = { AUP_TBOOL, true };
static const aupVal AUP_FALSE = { AUP_TBOOL, false };

#define AUP_BOOL(b)         ((aupVal){ AUP_TBOOL, .Bool = (b) })
#define AUP_NUM(n)          ((aupVal){ AUP_TNUM, .Num = (n) })
#define AUP_PTR(p)          ((aupVal){ AUP_TPTR, .Ptr = (void *)(p) })
#define AUP_CFN(c)          ((aupVal){ AUP_TCFN, .CFn = (c) })
#define AUP_OBJ(o)          ((aupVal){ AUP_TOBJ, .Obj = (aupObj *)(o) })

#define AUP_IS_NIL(v)       ((v).type == AUP_TNIL)
#define AUP_IS_BOOL(v)      ((v).type == AUP_TBOOL)
#define AUP_IS_NUM(v)       ((v).type == AUP_TNUM)
#define AUP_IS_CFN(v)       ((v).type == AUP_TCFN)
#define AUP_IS_PTR(v)       ((v).type == AUP_TPTR)
#define AUP_IS_OBJ(v)       ((v).type == AUP_TOBJ)

#define AUP_AS_BOOL(v)      ((v).Bool)
#define AUP_AS_NUM(v)       ((v).Num)
#define AUP_AS_OBJ(v)       ((v).Obj)
#define AUP_AS_CFN(v)       ((v).CFn)
#define AUP_AS_PTR(v)       ((v).Ptr)

#define AUP_AS_INT(v)       ((int)AUP_AS_NUM(v))
#define AUP_AS_INT64(v)     ((int64_t)AUP_AS_NUM(v))
#define AUP_AS_RAW(v)       ((v).Raw)

#if (defined(__clang_major__) &&  (__clang_major__ <= 6)) || (defined(_MSC_VER) && _MSC_VER <= 1600)
static inline bool AUP_IS_FALSEY(aupVal value) {
    switch (value.type) {
        case AUP_TNIL: return true;
        case AUP_TBOOL: return AUP_AS_BOOL(value) == false;
        case AUP_TNUM: return AUP_AS_NUM(value) == 0;
        case AUP_TCFN:
        case AUP_TPTR: return AUP_AS_PTR(value) == NULL;
        case AUP_TOBJ:
        default: return false;
    }
}
#else
#define AUP_IS_FALSEY(v)    (!(bool)AUP_AS_RAW(v))
#endif

void aup_printValue(aupVal value);
bool aup_valuesEqual(aupVal a, aupVal b);
const char *aup_typeofValue(aupVal value);

void aup_initArray(aupArr *array);
void aup_freeArray(aupArr *array);
int aup_pushArray(aupArr *array, aupVal value, bool allowdup);

#endif
