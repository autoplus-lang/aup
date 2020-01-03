#ifndef _AUP_OBJECT_H
#define _AUP_OBJECT_H
#pragma once

#include "value.h"
#include "chunk.h"

struct _aupObj {
    unsigned isMarked : 1;
	aupVt type : 8;
	struct _aupObj *next;
};

struct _aupOstr {
	aupObj obj;
	char *chars;
	int length;
	uint32_t hash;
};

struct _aupOupv {
    aupObj obj;
    aupVal *value;
    aupVal closed;
	struct _aupOupv *next;
};

struct _aupOfun {
    aupObj obj;
	int arity;
	int upvalueCount;
    aupChunk chunk;
	aupOstr *name;
	aupOupv **upvalues;
};

static inline bool aupO_isType(aupVal value, aupVt type) {
	return AUP_IS_OBJ(value) && AUP_AS_OBJ(value)->type == type;
}

#define AUP_OBJ_TYPE(v)	(AUP_AS_OBJ(v)->type)
#define AUP_IS_STR(v)	aupO_isType(v, AUP_TSTR)
#define AUP_IS_FUN(v)	aupO_isType(v, AUP_TFUN)

#define AUP_AS_STR(v)	((aupOstr *)AUP_AS_OBJ(v))
#define AUP_AS_CSTR(v)	(((aupOstr *)AUP_AS_OBJ(v))->chars)
#define AUP_AS_FUN(v)	((aupOfun *)AUP_AS_OBJ(v))

aupOstr *aup_takeString(AUP_VM, char *chars, int length);
aupOstr *aup_copyString(AUP_VM, const char *chars, int length);

aupOfun *aup_newFunction(AUP_VM);
void aup_makeClosure(aupOfun *function);

aupOupv *aup_newUpval(AUP_VM, aupVal *slot);

const char *aup_typeofObj(aupObj *object);
void aup_printObj(aupObj *object);

#endif
