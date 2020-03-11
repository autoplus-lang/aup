#ifndef _AUP_VALUE_H
#define _AUP_VALUE_H
#pragma once

#include "util.h"

typedef struct _aupObj aupObj;
typedef struct _aupStr aupStr;
typedef struct _aupFun aupFun;
typedef struct _aupUpv aupUpv;

typedef enum {
    AUP_TNIL,
    AUP_TBOOL,
    AUP_TNUM,
    AUP_TOBJ
} aupTVal;

typedef enum {
    AUP_OSTR,
    AUP_OFUN,
    AUP_OUPV
} aupTObj;

enum {
#define PAIR(x, y)  aup_cmb(AUP_T##x, AUP_T##y)

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

#define aup_vNil        (aupVal){ AUP_TNIL }
#define aup_vTrue       (aupVal){ AUP_TBOOL, .Bool = true }
#define aup_vFalse      (aupVal){ AUP_TBOOL, .Bool = false }

#define aup_vBool(b)    ((aupVal){ AUP_TBOOL, .Bool = (bool)(b) })
#define aup_vNum(n)     ((aupVal){ AUP_TNUM,  .Num = (double)(n) })
#define aup_vObj(o)     ((aupVal){ AUP_TOBJ,  .Obj = (aupObj *)(o) })

#define aup_asBool(v)   ((v).Bool)
#define aup_asNum(v)    ((v).Num)
#define aup_asObj(v)    ((v).Obj)
#define aup_asInt(v)    ((int)aup_asNum(v))
#define aup_asI64(v)    ((int64_t)aup_asNum(v))
#define aup_asRaw(v)    ((v).raw)

#define aup_typeof(v)   ((v).type)

#define aup_isNil(v)    (aup_typeof(v) == AUP_TNIL)
#define aup_isBool(v)   (aup_typeof(v) == AUP_TBOOL)
#define aup_isNum(v)    (aup_typeof(v) == AUP_TNUM)
#define aup_isObj(v)    (aup_typeof(v) == AUP_TOBJ)

#if (defined(_MSC_VER) && _MSC_VER <= 1600) || defined(__clang__)
static bool aup_isFalsey(aupVal v) {
    switch (aup_typeof(v)) {
        case AUP_TNIL:  return true;
        case AUP_TBOOL: return !aup_asBool(v);
        case AUP_TNUM:  return aup_asNum(v) == 0;
        case AUP_TOBJ:
        default: return false;
    }
}
#else
#define aup_isFalsey(v) (!(bool)aup_asRaw(v))
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
    int space;
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
