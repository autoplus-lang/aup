#ifndef _AUP_OBJECT_H
#define _AUP_OBJECT_H
#pragma once

#include "value.h"

struct _aupO {
	aupVt type;
	struct _aupO *next;
};

struct _aupOs {
	struct _aupO obj;
	char *chars;
	int length;
	uint32_t hash;
};

static inline bool aupO_isType(aupV value, aupVt type) {
	return AUP_IS_OBJ(value) && AUP_AS_OBJ(value)->type == type;
}

#define AUP_OBJ_TYPE(v)	(AUP_AS_OBJ(v)->type)
#define AUP_IS_STR(v)	aupO_isType(v, AUP_TSTR)	

#define AUP_AS_STR(v)	((aupOs *)AUP_AS_OBJ(v))         
#define AUP_AS_CSTR(v)	(((aupOs *)AUP_AS_OBJ(v))->chars)

aupOs *aupOs_take(AUP_VM, char *chars, int length);
aupOs *aupOs_copy(AUP_VM, const char *chars, int length);

const char *aupO_typeOf(aupO *object);
void aupO_print(aupO *object);

#endif
