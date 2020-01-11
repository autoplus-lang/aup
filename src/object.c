#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "gc.h"
#include "vm.h"

void aup_printObject(aupObj *object)
{
    switch (object->type) {
        case AUP_TSTR: {
            aupStr *string = (aupStr *)object;
            printf("%.*s", string->length, string->chars);
            break;
        }
        case AUP_TFUN: {
            aupFun *function = (aupFun *)object;
            if (function->name == NULL)
                printf("<script>");
            else
                printf("fn: %s", function->name->chars);
            break;
        }
        case AUP_TMAP:
            printf("map: %p", object);
            break;
        default:
            printf("obj: %p", object);
            break;
    }
}

const char *aup_typeofObject(aupObj *object)
{
    switch (object->type) {
        case AUP_TSTR:
            return "str";
        case AUP_TFUN:
            return "fn";
        default:
            return "obj";
    }
}

#define ALLOC(vm, size) \
    aup_realloc(vm, (vm)->gc, NULL, 0, size)

#define FREE(gc, type, pointer) \
    aup_realloc(NULL, gc, pointer, sizeof(type), 0)

#define ALLOC_OBJ(vm, type, objectType) \
    (type *)allocObj(vm, sizeof(type), objectType)

static aupObj *allocObj(aupVM *vm, size_t size, aupOType type)
{
    aupObj *object = ALLOC(vm, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->gc->objects;
    vm->gc->objects = object;
    return object;
}

static aupStr *allocStr(aupVM *vm, char *chars, int length, uint32_t hash)
{
    aupStr *string = ALLOC_OBJ(vm, aupStr, AUP_TSTR);
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    aup_pushRoot(vm, (aupObj *)string);
    aup_setTable(vm->strings, string, AUP_NIL);
    aup_popRoot(vm);
    return string;
}

aupStr *aup_takeString(aupVM *vm, char *chars, int length)
{
    uint32_t hash = aup_hashBytes(chars, length);
    aupStr *interned = aup_findString(vm->strings, chars, length, hash);
    if (interned != NULL) {
        free(chars);
        return interned;
    }

    return allocStr(vm, chars, length, hash);
}

aupStr *aup_copyString(aupVM *vm, const char *chars, int length)
{
    if (length < 0) length = (int)strlen(chars);

    uint32_t hash = aup_hashBytes(chars, length);
    aupStr *interned = aup_findString(vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char *heapChars = malloc((length + 1) * sizeof(char));
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocStr(vm, heapChars, length, hash);
}

aupFun *aup_newFunction(aupVM *vm, aupSrc *source)
{
    aupFun *function = ALLOC_OBJ(vm, aupFun, AUP_TFUN);

    function->arity = 0;
    function->upvalueCount = 0;
    function->upvalues = NULL;
    function->name = NULL;
    aup_initChunk(&function->chunk, source);

    return function;
}

void aup_makeClosure(aupFun *function)
{
    int upvalueCount = function->upvalueCount;
    aupUpv **upvalues = malloc(sizeof(aupUpv *) * upvalueCount);

    for (int i = 0; i < upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    function->upvalues = upvalues;
}

aupUpv *aup_newUpvalue(aupVM *vm, aupVal *slot)
{
    aupUpv *upvalue = ALLOC_OBJ(vm, aupUpv, AUP_TUPV);
    upvalue->location = slot;
    upvalue->next = NULL;

    return upvalue;
}

aupMap *aup_newMap(aupVM *vm)
{
    aupMap *map = ALLOC_OBJ(vm, aupMap, AUP_TMAP);

    aup_initHash(&map->hash);
    aup_initTable(&map->table);

    return map;
}

void aup_freeObject(aupGC *gc, aupObj *object)
{
    switch (object->type) {
        case AUP_TSTR: {
            aupStr *string = (aupStr *)object;
            free(string->chars);
            FREE(gc, aupStr, string);
            break;
        }
        case AUP_TFUN: {
            aupFun *function = (aupFun *)object;
            aup_freeChunk(&function->chunk);
            if (function->upvalueCount > 0) free(function->upvalues);
            FREE(gc, aupFun, function);
            break;
        }
        case AUP_TUPV: {
            FREE(gc, aupUpv, object);
            break;
        }
        case AUP_TMAP: {
            aupMap *map = (aupMap *)object;
            aup_freeHash(&map->hash);
            aup_freeTable(&map->table);
            FREE(gc, aupMap, map);
            break;
        }
    }
}
