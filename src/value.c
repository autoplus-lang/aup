#include <stdio.h>
#include <stdlib.h>

#include "value.h"
#include "object.h"
#include "memory.h"

static const char
	__nil[]  = "nil",
	__bool[] = "bool",
	__int[]  = "int",
	__num[]  = "num";

const char *aup_typeofVal(aupVal value)
{
	switch (AUP_TYPE(value)) {
		default:
		case AUP_TNIL:
			return __nil;
		case AUP_TBOOL:
			return __bool;
		case AUP_TINT:
			return __int;
		case AUP_TNUM:
			return __num;
		case AUP_TOBJ:
			return aup_typeofObj(AUP_AS_OBJ(value));
	}
}

void aup_printVal(aupVal value)
{
	switch (AUP_TYPE(value)) {
		default:
		case AUP_TNIL:
			printf("nil");
			break;
		case AUP_TBOOL:
			printf(AUP_AS_BOOL(value) ? "true" : "false");
			break;
		case AUP_TINT:
			printf("%lld", AUP_AS_INT(value));
			break;
		case AUP_TNUM:
			printf("%.14g", AUP_AS_NUM(value));
			break;
		case AUP_TOBJ:
			aup_printObj(AUP_AS_OBJ(value));
			break;
	}
}

void aup_initValueArr(aupValArr *array)
{
	array->count = 0;
	array->capacity = 0;
	array->values = NULL;
}

int aup_writeValueArr(aupValArr *array, aupVal value)
{
	if (array->capacity < array->count + 1) {
		int oldCapacity = array->capacity;
		array->capacity = AUP_GROW_CAP(oldCapacity);
		array->values = realloc(array->values, array->capacity * sizeof(aupVal));
	}

	array->values[array->count] = value;
	return array->count++;
}

void aup_freeValueArr(aupValArr *array)
{
	free(array->values);
	aup_initValueArr(array);
}

int aup_findValue(aupValArr *array, aupVal value)
{
#define IS_EQUAL(v1, v2) \
	AUP_TYPE(v1) == AUP_TYPE(v2) && \
	AUP_AS_RAW(v1) == AUP_AS_RAW(v2)

	for (int i = 0; i < array->count; i++) {
		if (IS_EQUAL(array->values[i], value)) {
			return i;
		}
	}

	return -1;
}
