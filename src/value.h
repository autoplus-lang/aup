#ifndef _AUP_VALUE_H
#define _AUP_VALUE_H
#pragma once

#include "util.h"

typedef struct _aupObj aupObj;
typedef struct _aupStr aupStr;
typedef struct _aupFun aupFun;
typedef struct _aupUpv aupUpv;
typedef struct _aupClass aupClass;

typedef enum {
    AUP_TNIL,
    AUP_TBOOL,
    AUP_TNUM,
    AUP_TOBJ
} aupTVal;

typedef enum {
    AUP_OSTR,
    AUP_OFUN,
    AUP_OUPV,
    AUP_OCLASS,
} aupTObj;

enum {
#define PAIR(a, b)  AUP_PAIR(AUP_T##a, AUP_T##b)

    AUP_TNIL_NIL    = PAIR(NIL, NIL),
    AUP_TNIL_BOOL   = PAIR(NIL, BOOL),
    AUP_TNIL_NUM    = PAIR(NIL, NUM),
    AUP_TNIL_OBJ    = PAIR(NIL, OBJ),

    AUP_TBOOL_NIL   = PAIR(BOOL, NIL),
    AUP_TBOOL_BOOL  = PAIR(BOOL, BOOL),
    AUP_TBOOL_NUM   = PAIR(BOOL, NUM),
    AUP_TBOOL_OBJ   = PAIR(BOOL, OBJ),

    AUP_TNUM_NIL    = PAIR(NUM, NIL),
    AUP_TNUM_BOOL   = PAIR(NUM, BOOL),
    AUP_TNUM_NUM    = PAIR(NUM, NUM),
    AUP_TNUM_OBJ    = PAIR(NUM, OBJ),

    AUP_TOBJ_NIL    = PAIR(OBJ, NIL),
    AUP_TOBJ_BOOL   = PAIR(OBJ, BOOL),
    AUP_TOBJ_NUM    = PAIR(OBJ, NUM),
    AUP_TOBJ_OBJ    = PAIR(OBJ, OBJ),

#undef PAIR
};

struct _aupVal {
    aupTVal type;
    union {
        bool     Bool : 1;
        double   Num;
        aupObj   *Obj;
        uint64_t raw;
    };
};

#define AUP_VNil        ((aupVal){ AUP_TNIL })
#define AUP_VTrue       ((aupVal){ AUP_TBOOL, .Bool = true })
#define AUP_VFalse      ((aupVal){ AUP_TBOOL, .Bool = false })

#define AUP_VBool(b)    ((aupVal){ AUP_TBOOL, .Bool = (bool)(b) })
#define AUP_VNum(n)     ((aupVal){ AUP_TNUM,  .Num = (double)(n) })
#define AUP_VObj(o)     ((aupVal){ AUP_TOBJ,  .Obj = (aupObj *)(o) })

#define AUP_AsBool(v)   ((v).Bool)
#define AUP_AsNum(v)    ((v).Num)
#define AUP_AsObj(v)    ((v).Obj)
#define AUP_AsInt(v)    ((int)AUP_AsNum(v))
#define AUP_AsI64(v)    ((int64_t)AUP_AsNum(v))
#define AUP_AsRaw(v)    ((v).raw)

#define AUP_Typeof(v)   ((v).type)

#define AUP_IsNil(v)    (AUP_Typeof(v) == AUP_TNIL)
#define AUP_IsBool(v)   (AUP_Typeof(v) == AUP_TBOOL)
#define AUP_IsNum(v)    (AUP_Typeof(v) == AUP_TNUM)
#define AUP_IsObj(v)    (AUP_Typeof(v) == AUP_TOBJ)

#if (defined(_MSC_VER) && _MSC_VER <= 1600) || defined(__clang__)
static bool AUP_IsFalsey(aupVal v) {
    switch (v.type) {
        case AUP_TNIL:  return true;
        case AUP_TBOOL: return !aup_asBool(v);
        case AUP_TNUM:  return aup_asNum(v) == 0;
        case AUP_TOBJ:
        default: return false;
    }
}
#else
#define AUP_IsFalsey(v) (!(bool)AUP_AsRaw(v))
#endif

void aup_printValue(aupVal val);
const char *aup_typeName(aupVal val);
bool aup_isEqual(aupVal a, aupVal b);

// Array list
typedef struct {
    int count;
    int space;
    aupVal *values;
} aupArr;

void aup_initArray(aupArr *arr);
void aup_freeArray(aupArr *arr);
int  aup_pushArray(aupArr *arr, aupVal val, bool allow_dup);

// Hash table
typedef struct {
    aupStr *key;
    aupVal value;
} aupEnt;

typedef struct {
    int count;
    int capMask;
    aupEnt *entries;
} aupTab;

void aup_initTable(aupTab *table);
void aup_freeTable(aupTab *table);
bool aup_getKey(aupTab *table, aupStr *key, aupVal *value);
bool aup_setKey(aupTab *table, aupStr *key, aupVal value);
bool aup_removeKey(aupTab *table, aupStr *key);
void aup_copyTable(aupTab *from, aupTab *to);
aupStr *aup_findString(aupTab *table, int length, uint32_t hash);

#endif
