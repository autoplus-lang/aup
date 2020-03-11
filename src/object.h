#ifndef _AUP_OBJECT_H
#define _AUP_OBJECT_H
#pragma once

#include "value.h"
#include "code.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#endif

// Size always is 8 bytes
struct _aupObj {
#ifdef AUP_X64
    union {
        // Pointer tag 0000FFFFFFFFFFFF
        uintptr_t next : 48;
        struct {
            unsigned : 32;
            unsigned : 24;
#else
            uintptr_t next;
#endif
            aupTObj type : 7;
            unsigned isMarked : 1;
#ifdef AUP_X64
        };
    };
#endif
};

struct _aupStr {
    aupObj base;
    int length;
    uint32_t hash;
    char *chars;
};

struct _aupFun {
    aupObj base;
    aupStr *name;
    aupUpv **upvals;
    int arity;
    int upvalCount;
    aupChunk chunk;
};

struct _aupUpv {
    aupObj base;
    aupUpv *next;
    aupVal *location;
    aupVal closed; 
};

#define aup_asStr(v)    ((aupStr *)aup_asObj(v))
#define aup_asCStr(v)   (aup_asStr(v)->chars)
#define aup_asFun(v)    ((aupFun *)aup_asObj(v))

#define aup_objType(v)  (aup_asObj(v)->type)

static inline bool aup_checkObj(aupVal val, aupTObj type) {
    return aup_isObj(val) && aup_objType(val) == type;
}

#define aup_isStr(v)    (aup_checkObj(v, AUP_OSTR))
#define aup_isFun(v)    (aup_checkObj(v, AUP_OFUN))

void aup_printObject(aupObj *object);
void aup_freeObject(aupGC *gc, aupObj *object);

aupStr *aup_takeString(aupVM *vm, char *chars, int length);
aupStr *aup_copyString(aupVM *vm, const char *chars, int length);
aupStr *aup_catString(aupVM *vm, aupStr *s1, aupStr *s2);

aupFun *aup_newFunction(aupVM *vm, aupSrc *source);
void aup_makeClosure(aupFun *function);

#endif
