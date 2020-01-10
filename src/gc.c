#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "object.h"
#include "vm.h"
#include "table.h"
#include "code.h"

void aup_initGC(aupGC *gc)
{
    gc->allocated = 0;
    gc->nextGC = 1024 * 1024;
    gc->objects = NULL;

    gc->grayCount = 0;
    gc->grayCapacity = 0;
    gc->grayStack = NULL;
}

void aup_freeGC(aupGC *gc)
{
    aupObj *object = gc->objects;

    while (object != NULL) {
        aupObj *next = object->next;
        aup_freeObject(gc, object);
        object = next;
    }

    free(gc->grayStack);
}

void *aup_realloc(aupVM *vm, aupGC *gc, void *ptr, size_t old, size_t new)
{
    gc->allocated += new - old;

    if (new > old && gc->allocated > gc->nextGC) {
        aup_collect(vm);
    }

    if (new == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, new);
}

void aup_markObject(aupVM *vm, aupObj *object)
{
    if (object == NULL) return;
    if (object->isMarked) return;
    object->isMarked = true;

    aupGC *gc = vm->gc;

    if (gc->grayCapacity < gc->grayCount + 1) {
        gc->grayCapacity = AUP_GROWCAP(gc->grayCapacity);
        gc->grayStack = realloc(gc->grayStack,
            sizeof(aupObj *) * gc->grayCapacity);
    }

    gc->grayStack[gc->grayCount++] = object;
}

void aup_markValue(aupVM *vm, aupVal value)
{
    if (!AUP_IS_OBJ(value)) return;
    aup_markObject(vm, AUP_AS_OBJ(value));
}

static void markArray(aupVM *vm, aupArr *array)
{
    for (int i = 0; i < array->count; i++) {
        aup_markValue(vm, array->values[i]);
    }
}

static void blackenObject(aupVM *vm, aupObj *object)
{
    switch (object->type) {
        case AUP_TSTR:
            break;
        case AUP_TUPV:
            aup_markValue(vm, ((aupUpv *)object)->closed);
            break;
        case AUP_TFUN: {
            aupFun *function = (aupFun *)object;
            aup_markObject(vm, (aupObj *)function->name);
            markArray(vm, &function->chunk.constants);
            for (int i = 0; i < function->upvalueCount; i++) {
                aup_markObject(vm, (aupObj*)function->upvalues[i]);
            }
            break;
        }
        case AUP_TMAP: {
            aupMap *map = (aupMap *)object;
            aup_markTable(vm, &map->table);
            aup_markHash(vm, &map->hash);
            break;
        }
    }
}

static void markRoots(aupVM *vm)
{
    for (aupVal *slot = vm->stack; slot < vm->top; slot++) {
        aup_markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        aup_markObject(vm, (aupObj *)vm->frames[i].function);
    }

    for (aupUpv *upvalue = vm->openUpvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
        aup_markObject(vm, (aupObj *)upvalue);
    }

    aup_markTable(vm, vm->globals);
    aup_markCompilerRoots(vm);
}

static void traceReferences(aupVM *vm)
{
    aupGC *gc = vm->gc;

    while (gc->grayCount > 0) {
        aupObj *object = gc->grayStack[--gc->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(aupVM *vm)
{
    aupGC *gc = vm->gc;

    aupObj *previous = NULL;
    aupObj *object = gc->objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        }
        else {
            aupObj *unreached = object;

            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            }
            else {
                gc->objects = object;
            }

            aup_freeObject(gc, unreached);
        }
    }
}

void aup_collect(aupVM *vm)
{
    aupGC *gc = vm->gc;

    markRoots(vm);
    traceReferences(vm);
    aup_tableRemoveWhite(vm->strings);
    sweep(vm);

    gc->nextGC = gc->allocated * 2;
}
