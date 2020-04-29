#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "value.h"
#include "object.h"

void aup_printObject(aupObj *object)
{
    switch (object->type) {
        case AUP_OSTR: {
            aupStr *string = (aupStr *)object;
            printf("%.*s", string->length, string->chars);
            break;
        }
        case AUP_OFUN: {
            aupFun *function = (aupFun *)object;
            if (function->name == NULL)
                printf("<script>");
            else
                printf("func: %s@%p", function->name->chars, function);
            break;
        }
        case AUP_OKLS: {
            aupKls *klass = (aupKls *)object;
            printf("class: %s@%p", klass->name->chars, klass);
            break;
        }
        case AUP_OINC: {
            aupInc *instance = (aupInc *)object;
            printf("instance: %s@%p", instance->klass->name->chars, instance);
            break;
        }
        default:
            printf("obj: %p", object);
                break;
    }
}

#define ALLOC(size) \
    aup_alloc(size)
#define FREE(ptr, type) \
    aup_dealloc(ptr, sizeof(type))
#define FREE_ARR(ptr, type, count) \
    aup_dealloc(ptr, sizeof(type) * (count))

#define ALLOC_OBJ(t, ot) \
    (t *)aup_allocObject(sizeof(t), ot)

static aupStr *allocString(char *chars, int length, uint32_t hash)
{
    aupStr *string = ALLOC_OBJ(aupStr, AUP_OSTR);
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    //AUP_PushRoot(vm, (aupObj *)string);
    aup_setKey(aup_getStrings(), string, AUP_VNil);
    //AUP_PopRoot(vm);
    return string;
}

aupStr *aup_catString(aupStr *s1, aupStr *s2)
{
    int l1 = s1->length;
    int l2 = s2->length;
    int length = l1 + l2;
    const char *cs1 = s1->chars;
    const char *cs2 = s2->chars;

    uint32_t hash = aup_hashBytes(2, cs1, l1, cs2, l2);
    aupStr *interned = aup_findString(aup_getStrings(), length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOC((length + 1) * sizeof(char));
    memcpy(heapChars, cs1, l1);
    memcpy(heapChars + l1 , cs2, l2);
    heapChars[length] = '\0';

    return allocString(heapChars, length, hash);
}

aupStr *aup_takeString(char *chars, int length)
{
    uint32_t hash = aup_hashBytes(1, chars, length);
    aupStr *interned = aup_findString(aup_getStrings(), length, hash);
    if (interned != NULL) {
        FREE_ARR(chars, char, length);
        return interned;
    }

    return allocString(chars, length, hash);
}

aupStr *aup_copyString(const char *chars, int length)
{
    if (length < 0) length = (int)strlen(chars);

    uint32_t hash = aup_hashBytes(1, chars, length);
    aupStr *interned = aup_findString(aup_getStrings(), length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOC((length + 1) * sizeof(char));
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocString(heapChars, length, hash);
}

aupFun *aup_newFunction(aupSrc *source)
{
    aupFun *function = ALLOC_OBJ(aupFun, AUP_OFUN);

    function->arity = 0;
    function->upvalCount = 0;
    function->upvals = NULL;
    function->name = NULL;
    aup_initChunk(&function->chunk, source);

    return function;
}

void aup_makeClosure(aupFun *function)
{
    size_t size = function->upvalCount * sizeof(aupUpv *);
    aupUpv **upvals = malloc(size);
    memset(upvals, '\0', size);
    function->upvals = upvals;
}

aupUpv *aup_newUpval(aupVal *slot)
{
    aupUpv *upval = ALLOC_OBJ(aupUpv, AUP_OUPV);
    upval->closed = AUP_VNil;
    upval->location = slot;
    upval->next = NULL;

    return upval;
}

aupKls *aup_newClass(aupStr *name)
{
    aupKls *klass = ALLOC_OBJ(aupKls, AUP_OKLS);
    klass->name = name;
    return klass;
}

aupInc *aup_newInstance(aupKls *klass)
{
    aupInc *instance = ALLOC_OBJ(aupInc, AUP_OINC);
    instance->klass = klass;
    aup_initTable(&instance->fields);
    return instance;
}

void aup_freeObject(aupObj *object)
{
    switch (object->type) {
        case AUP_OSTR: {
            aupStr *string = (aupStr *)object;
            FREE_ARR(string->chars, char, string->length);
            FREE(string, aupStr);
            break;
        }
        case AUP_OFUN: {
            aupFun *function = (aupFun *)object;
            aup_freeChunk(&function->chunk);
            if (function->upvalCount > 0) free(function->upvals);
            FREE(function, aupFun);
            break;
        }
        case AUP_OUPV: {
            FREE(object, aupUpv);
            break;
        }
        case AUP_OKLS: {
            FREE(object, aupKls);
            break;
        }
        case AUP_OINC: {
            aupInc *instance = (aupInc *)object;
            aup_freeTable(&instance->fields);
            FREE(object, aupInc);
            break;
        }
    }
}
