#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"

void aup_printValue(aupVal val)
{
    switch (val.type) {
        case AUP_TNIL:
        default:
            printf("nil");
            break;
        case AUP_TBOOL:
            printf(AUP_AsBool(val) ? "true" : "false");
            break;
        case AUP_TNUM:
            printf("%.14g", AUP_AsNum(val));
            break;
        case AUP_TOBJ:
            aup_printObject(AUP_AsObj(val));
            break;
    }
}

const char *aup_typeName(aupVal val)
{
    switch (val.type) {
        case AUP_TNIL:
            return "nil";
        case AUP_TBOOL:
            return "bool";
        case AUP_TNUM:
            return "num";
        case AUP_TOBJ: {
            switch (AUP_OType(val)) {
                case AUP_OSTR:
                    return "str";
                case AUP_OFUN:
                    return "fun";
            }
        }
    }

    return "nil";
}

bool aup_isEqual(aupVal a, aupVal b)
{
    if (a.type == b.type) {
        switch (a.type) {
            case AUP_TNIL:
                return true;
            case AUP_TBOOL:
                return AUP_AsBool(a) == AUP_AsBool(b);
            case AUP_TNUM:
                return AUP_AsNum(a) == AUP_AsNum(b);
            case AUP_TOBJ:
                return AUP_AsObj(a) == AUP_AsObj(b);
            default:
                return false;
        }
    }
    else if (AUP_IsNum(a) && AUP_IsBool(b)) {
        return AUP_AsNum(a) == AUP_AsBool(b);
    }
    else if (AUP_IsBool(a) && AUP_IsNum(b)) {
        return AUP_AsBool(a) == AUP_AsNum(b);
    }

    return false;
}

void aup_initArray(aupArr *arr)
{
    memset(arr, '\0', sizeof(*arr));
}

void aup_freeArray(aupArr *arr)
{
    free(arr->values);
}

int aup_pushArray(aupArr *arr, aupVal val, bool allow_dup)
{
    if (!allow_dup) {
        for (int i = 0; i < arr->count; i++)
            if (aup_isEqual(arr->values[i], val))
                return i;
    }

    if (arr->count >= arr->space) {
        arr->space = AUP_GROW(arr->space);
        arr->values = realloc(arr->values, arr->space * sizeof(aupVal));
    }

    arr->values[arr->count] = val;
    return arr->count++;
}
