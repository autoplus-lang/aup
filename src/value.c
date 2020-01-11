#include <stdio.h>
#include <stdlib.h>

#include "value.h"
#include "object.h"

void aup_printValue(aupVal value)
{
    switch (value.type) {
        case AUP_TNIL:
        default:
            printf("nil");
            break;
        case AUP_TBOOL:
            printf(AUP_AS_BOOL(value) ? "true" : "false");
            break;
        case AUP_TNUM:
            printf("%.14g", AUP_AS_NUM(value));
            break;
        case AUP_TCFN:
            printf("fn: %p", AUP_AS_CFN(value));
            break;
        case AUP_TPTR:
            printf("ptr: %p", AUP_AS_PTR(value));
            break;
        case AUP_TOBJ:
            aup_printObject(AUP_AS_OBJ(value));
            break;
    }

    fflush(stdout);
}

bool aup_valuesEqual(aupVal a, aupVal b)
{
    if (a.type != b.type) return false;
    switch (a.type) {
        case AUP_TNIL:
            return true;
        case AUP_TBOOL:
            return AUP_AS_BOOL(a) == AUP_AS_BOOL(b);
        case AUP_TNUM:
            return AUP_AS_NUM(a) == AUP_AS_NUM(b);
        case AUP_TCFN:
            return AUP_AS_CFN(a) == AUP_AS_CFN(b);
        case AUP_TPTR:
            return AUP_AS_PTR(a) == AUP_AS_PTR(b);
        case AUP_TOBJ:
            return AUP_AS_OBJ(a) == AUP_AS_OBJ(b);
        default:
            return false;
    }
}

const char *aup_typeofValue(aupVal value)
{
    switch (value.type) {        
        case AUP_TNIL:
        default:
            return "nil";
        case AUP_TBOOL:
            return "bool";
        case AUP_TNUM:
            return "num";
        case AUP_TPTR:
            return "ptr";
        case AUP_TCFN:
            return "fn";
        case AUP_TOBJ:
            return aup_typeofObject(AUP_AS_OBJ(value));
    }
}

void aup_initArray(aupArr *array)
{
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void aup_freeArray(aupArr *array)
{
    free(array->values);
    aup_initArray(array);
}

int aup_pushArray(aupArr *array, aupVal value, bool allowdup)
{
    if (!allowdup) {
        for (int i = 0; i < array->count; i++)
            if (aup_valuesEqual(array->values[i], value))
                return i;
    }

    if (array->count >= array->capacity) {
        array->capacity = AUP_GROWCAP(array->capacity);
        array->values = realloc(array->values, array->capacity * sizeof(aupVal));
    }

    array->values[array->count] = value;
    return array->count++;
}
