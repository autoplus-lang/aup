#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"

void aup_printValue(aupVal val)
{
    switch (aup_typeof(val)) {
        case AUP_TNIL:
        default:
            printf("nil");
            break;
        case AUP_TBOOL:
            printf(aup_asBool(val) ? "true" : "false");
            break;
        case AUP_TNUM:
            printf("%.14g", aup_asNum(val));
            break;
        case AUP_TOBJ:
            aup_printObject(aup_asObj(val));
            break;
    }
}

const char *aup_typeName(aupVal val)
{
    switch (aup_typeof(val)) {
        case AUP_TNIL:
            return "nil";
        case AUP_TBOOL:
            return "bool";
        case AUP_TNUM:
            return "num";
        case AUP_TOBJ: {
            switch (aup_objType(val)) {
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
    if (aup_typeof(a) == aup_typeof(b)) {
        switch (aup_typeof(a)) {
            case AUP_TNIL:
                return true;
            case AUP_TBOOL:
                return aup_asBool(a) == aup_asBool(b);
            case AUP_TNUM:
                return aup_asNum(a) == aup_asNum(b);
            case AUP_TOBJ:
                return aup_asObj(a) == aup_asObj(b);
            default:
                return false;
        }
    }
    else if (aup_isNum(a) && aup_isBool(b)) {
        return aup_asNum(a) == aup_asBool(b);
    }
    else if (aup_isBool(a) && aup_isNum(b)) {
        return aup_asBool(a) == aup_asNum(b);
    }

    return false;
}

aupStr *aup_toString(aupVM *vm, aupVal val)
{
    switch (aup_typeof(val)) {
        case AUP_TNIL:
        case AUP_TBOOL:
        case AUP_TNUM:
        case AUP_TOBJ: {
            aupObj *object = aup_asObj(val);
            switch (object->type) {
                case AUP_OSTR:
                    return (aupStr *)object;
                case AUP_OFUN:
                    return ((aupFun *)object)->name;
            }
        }
    }

    return NULL;
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
        arr->space = aup_growCap(arr->space);
        arr->values = realloc(arr->values, arr->space * sizeof(aupVal));
    }

    arr->values[arr->count] = val;
    return arr->count++;
}
