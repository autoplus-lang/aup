#ifndef _AUP_OBJECT_H
#define _AUP_OBJECT_H
#pragma once

#include "value.h"
#include "code.h"

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
    char  *chars;
    int    length;
    uint32_t hash; 
};

struct _aupFun {
    aupObj base;
    aupStr *name;
    aupUpv **upvals;
    int    arity;
    int    upvalCount;
    aupChunk chunk;
    int    locals;
};

struct _aupUpv {
    aupObj base;
    aupUpv *next;
    aupVal *location;
    aupVal closed; 
};

struct _aupKls {
    aupObj base;
    aupStr *name;
};

struct _aupInc {
    aupObj base;
    aupKls *klass;
    aupTab fields;
};

#define AUP_AsStr(v)    ((aupStr *)AUP_AsObj(v))
#define AUP_AsCStr(v)   (AUP_AsStr(v)->chars)
#define AUP_AsFun(v)    ((aupFun *)AUP_AsObj(v))
#define AUP_AsClass(v)  ((aupKls *)AUP_AsObj(v))

#define AUP_OType(v)    (AUP_AsObj(v)->type)

static inline bool AUP_CheckObj(aupVal val, aupTObj type) {
    return AUP_IsObj(val) && AUP_OType(val) == type;
}

#define AUP_IsStr(v)    (AUP_CheckObj(v, AUP_OSTR))
#define AUP_IsFun(v)    (AUP_CheckObj(v, AUP_OFUN))
#define AUP_IsClass(v)  (AUP_CheckObj(v, AUP_OKLS))

void aup_printObject(aupObj *object);
void aup_freeObject(aupGC *gc, aupObj *object);

aupStr *aup_takeString(aupVM *vm, char *chars, int length);
aupStr *aup_copyString(aupVM *vm, const char *chars, int length);
aupStr *aup_catString(aupVM *vm, aupStr *s1, aupStr *s2);

aupFun *aup_newFunction(aupVM *vm, aupSrc *source);
void aup_makeClosure(aupFun *function);

aupUpv *aup_newUpval(aupVM *vm, aupVal *slot);

aupKls *aup_newClass(aupVM *vm, aupStr *name);
aupInc *aup_newInstance(aupVM *vm, aupKls *klass);

#endif
