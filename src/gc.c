#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "vm.h"

void aup_initGC(aupGC *gc)
{
    gc->allocated = 0;
    gc->nextGC = 1024 * 1024;
    
    gc->grayCount = 0;
    gc->graySpace = 0;
    gc->grayStack = NULL;
    gc->objects = NULL;

    aup_initTable(&gc->strings);
    aup_initTable(&gc->globals);
}

void aup_freeGC(aupGC *gc)
{
    aupObj *object = gc->objects;

    while (object != NULL) {
        aupObj *next = (aupObj *)object->next;
        aup_freeObject(gc, object);
        object = next;
    }

    free(gc->grayStack);
    aup_freeTable(&gc->globals);
    aup_freeTable(&gc->strings);
}

void *aup_realloc(aupVM *vm, aupGC *gc, void *ptr, size_t old, size_t _new)
{
    gc->allocated += _new - old;

    if (_new > old && gc->allocated > gc->nextGC) {
        aup_collect(vm);
    }

    if (_new == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, _new);
}

static void markObject(aupGC *gc, aupObj *object)
{
    if (object == NULL) return;
    if (object->isMarked) return;
    object->isMarked = true;

    if (gc->graySpace <= gc->grayCount) {
        gc->graySpace = AUP_GROW(gc->graySpace);
        gc->grayStack = realloc(gc->grayStack,
            sizeof(aupObj *) * gc->graySpace);
    }

    gc->grayStack[gc->grayCount++] = object;
}

static void markValue(aupGC *gc, aupVal value)
{
    if (!AUP_IsObj(value)) return;
    markObject(gc, AUP_AsObj(value));
}

static void markArray(aupGC *gc, aupArr *array)
{
    for (int i = 0; i < array->count; i++) {
        markValue(gc, array->values[i]);
    }
}

static void markTable(aupGC *gc, aupTab *table)
{
    for (int i = 0; i <= table->capMask; i++) {
        aupEnt *entry = &table->entries[i];
        markObject(gc, (aupObj *)entry->key);
        markValue(gc, entry->value);
    }
}

void aup_collect(aupVM *vm)
{
    aupGC *gc = vm->gc;
    aupTab *strings = &gc->strings;
    aupTab *globals = &gc->globals;

    /* === Suspend all threads === */
    for (aupVM *next = vm->next;
        next != vm;
        next = next->next)
    {
        // TODO
    }

    /* === Mark roots === */
    for (int i = 0; i < vm->numRoots; i++)
    {
        markObject(gc, vm->tempRoots[i]);
    }

    // Mark stack
    aupVal *top = vm->top +
        vm->frames[vm->frameCount - 1].function->locals;
    for (aupVal *slot = vm->stack; slot < top; slot++)
    {
        markValue(gc, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++)
    {
        markObject(gc, (aupObj *)vm->frames[i].function);
    }

    for (aupUpv *upvalue = vm->openUpvals;
         upvalue != NULL;
         upvalue = upvalue->next)
    {
        markObject(gc, (aupObj *)upvalue);
    }

    markTable(gc, globals);
    //markCompilerRoots(vm);

    /* === Trace references === */
    while (gc->grayCount > 0)
    {
        aupObj *object = gc->grayStack[--gc->grayCount];
        switch (object->type) {
            case AUP_OSTR:
                break;
            case AUP_OUPV: {
                markValue(gc, ((aupUpv *)object)->closed);
                break;
            }
            case AUP_OFUN: {
                aupFun *function = (aupFun *)object;
                markObject(gc, (aupObj *)function->name);
                markArray(gc, &function->chunk.constants);
                for (int i = 0; i < function->upvalCount; i++) {
                    markObject(gc, (aupObj *)function->upvals[i]);
                }
                break;
            }
            case AUP_OCLASS: {
                aupClass *klass = (aupClass *)object;
                markObject(gc, (aupObj *)klass->name);
                break;
            }
        }
    }

    /* === Remove unreferenced strings === */
    for (int i = 0; i <= strings->capMask; i++)
    {
        aupStr *key = strings->entries[i].key;      
        if (key != NULL && !key->base.isMarked)
        {
            aup_removeKey(strings, key);
        }
    }

    /* === Sweep === */
    for (aupObj *previous = NULL,
                *object = gc->objects;
        object != NULL;)
    {
        if (object->isMarked)
        {
            object->isMarked = false;
            previous = object;
            object = (aupObj *)object->next;
        }
        else
        {
            aupObj *unreached = object;
            object = (aupObj *)object->next;

            if (previous != NULL)
            {
                previous->next = (uintptr_t)object;
            }
            else
            {
                gc->objects = object;
            }

            aup_freeObject(gc, unreached);
        }
    }

    gc->nextGC = gc->allocated * 2;

    /* === Resume all threads === */
    for (aupVM *next = vm->next;
        next != vm;
        next = next->next)
    {
        // TODO
    }
}
