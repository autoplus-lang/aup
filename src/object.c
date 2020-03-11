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
                printf("fn: %s", function->name->chars);
            break;
        }
        default:
            printf("obj: %p", object);
                break;
    }
}

#define ALLOC(vm, size) \
    aup_realloc(vm, (vm)->gc, NULL, 0, size)
#define FREE(gc, ptr, type) \
    aup_realloc(NULL, gc, ptr, sizeof(type), 0)
#define FREE_ARR(gc, ptr, type, count) \
    aup_realloc(NULL, gc, ptr, sizeof(type) * (count), 0)

#define ALLOC_OBJ(vm, t, ot) \
    (t *)allocObject(vm, sizeof(t), ot)

static aupObj *allocObject(aupVM *vm, size_t size, aupTObj type)
{
    aupObj *object = ALLOC(vm, size);
    object->type = type;
    object->isMarked = false;

    object->next = (uintptr_t)vm->gc->objects;
    vm->gc->objects = object;
    return object;
}

static aupStr *allocString(aupVM *vm, char *chars, int length, uint32_t hash)
{
    aupStr *string = ALLOC_OBJ(vm, aupStr, AUP_OSTR);
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    aup_pushRoot(vm, (aupObj *)string);
    aup_setKey(&vm->gc->strings, string, aup_vNil);
    aup_popRoot(vm);
    return string;
}

aupStr *aup_catString(aupVM *vm, aupStr *s1, aupStr *s2)
{
    int l1 = s1->length;
    int l2 = s2->length;
    int length = l1 + l2;
    const char *cs1 = s1->chars;
    const char *cs2 = s2->chars;

    uint32_t hash = aup_hashBytes(2, cs1, l1, cs2, l2);
    aupStr *interned = aup_findString(&vm->gc->strings, length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOC(vm, (length + 1) * sizeof(char));
    memcpy(heapChars, cs1, l1);
    memcpy(heapChars + l1 , cs2, l2);
    heapChars[length] = '\0';

    return allocString(vm, heapChars, length, hash);
}

aupStr *aup_takeString(aupVM *vm, char *chars, int length)
{
    uint32_t hash = aup_hashBytes(1, chars, length);
    aupStr *interned = aup_findString(&vm->gc->strings, length, hash);
    if (interned != NULL) {
        FREE_ARR(vm->gc, chars, char, length);
        return interned;
    }

    return allocString(vm, chars, length, hash);
}

aupStr *aup_copyString(aupVM *vm, const char *chars, int length)
{
    if (length < 0) length = (int)strlen(chars);

    uint32_t hash = aup_hashBytes(1, chars, length);
    aupStr *interned = aup_findString(&vm->gc->strings, length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOC(vm, (length + 1) * sizeof(char));
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocString(vm, heapChars, length, hash);
}

aupFun *aup_newFunction(aupVM *vm, aupSrc *source)
{
    aupFun *function = ALLOC_OBJ(vm, aupFun, AUP_OFUN);

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

aupUpv *aup_newUpval(aupVM *vm, aupVal *slot)
{
    aupUpv *upval = ALLOC_OBJ(vm, aupUpv, AUP_OUPV);
    upval->closed = aup_vNil;
    upval->location = slot;
    upval->next = NULL;

    return upval;
}

void aup_freeObject(aupGC *gc, aupObj *object)
{
    switch (object->type) {
        case AUP_OSTR: {
            aupStr *string = (aupStr *)object;
            FREE_ARR(gc, string->chars, char, string->length);
            FREE(gc, string, aupStr);
            break;
        }
        case AUP_OFUN: {
            aupFun *function = (aupFun *)object;
            aup_freeChunk(&function->chunk);
            if (function->upvalCount > 0) free(function->upvals);
            FREE(gc, function, aupFun);
            break;
        }
        case AUP_OUPV: {
            FREE(gc, object, aupUpv);
            break;
        }
    }
}
