#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "vm.h"

static struct {
    size_t nextGC;
    size_t allocated;
    aupObj *objects;
    aupObj **grayStack;
    int    grayCount;
    int    graySpace;
    aupTab strings;
    aupTab globals;
    aupVM  *root;
} m_gc;

void aup_initGC()
{
    m_gc.allocated = 0;
    m_gc.nextGC = 1024 * 1024;

    m_gc.grayCount = 0;
    m_gc.graySpace = 0;
    m_gc.grayStack = NULL;
    m_gc.objects = NULL;

    aup_initTable(&m_gc.strings);
    aup_initTable(&m_gc.globals);
}

void aup_freeGC()
{
    aupObj *object = m_gc.objects;

    while (object != NULL) {
        aupObj *next = (aupObj *)object->next;
        aup_freeObject(object);
        object = next;
    }

    free(m_gc.grayStack);
    aup_freeTable(&m_gc.globals);
    aup_freeTable(&m_gc.strings);
}

aupTab *aup_getStrings()
{
    return &m_gc.strings;
}

aupTab *aup_getGlobals()
{
    return &m_gc.globals;
}

void *aup_alloc(size_t size)
{
    m_gc.allocated += size;

    if (m_gc.allocated > m_gc.nextGC) {
        aup_collect();
    }

    return malloc(size);
}

void *aup_realloc(void *ptr, size_t old, size_t _new)
{
    m_gc.allocated += _new - old;

    if (_new > old && m_gc.allocated > m_gc.nextGC) {
        aup_collect();
    }

    if (_new == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, _new);
}

void aup_dealloc(void *ptr, size_t size)
{
    m_gc.allocated -= size;
    free(ptr);
}

void *aup_allocObject(size_t size, aupTObj type)
{
    aupObj *object = aup_alloc(size);
    object->type = type;
    object->isMarked = false;

    object->next = (uintptr_t)m_gc.objects;
    m_gc.objects = object;
    return object;
}

static void markObject(aupObj *object)
{
    if (object == NULL) return;
    if (object->isMarked) return;
    object->isMarked = true;

    if (m_gc.graySpace <= m_gc.grayCount) {
        m_gc.graySpace = AUP_GROW(m_gc.graySpace);
        m_gc.grayStack = realloc(m_gc.grayStack,
            sizeof(aupObj *) * m_gc.graySpace);
    }

    m_gc.grayStack[m_gc.grayCount++] = object;
}

static void markValue(aupVal value)
{
    if (!AUP_IsObj(value)) return;
    markObject(AUP_AsObj(value));
}

static void markArray(aupArr *array)
{
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

static void markTable(aupTab *table)
{
    for (int i = 0; i <= table->capMask; i++) {
        aupEnt *entry = &table->entries[i];
        markObject((aupObj *)entry->key);
        markValue(entry->value);
    }
}

void aup_collect()
{
    aupTab *strings = &m_gc.strings;
    aupTab *globals = &m_gc.globals;

    /* === Suspend all threads === */
    /*
    for (aupVM *next = vm->next;
        next != vm;
        next = next->next)
    {
        // TODO
    }
    */

    /* === Mark roots === */
    for (aupVM *vm = m_gc.root;
        vm != NULL;
        vm = vm->next)
    {
        // Mark temp objects
        for (int i = 0; i < vm->numRoots; i++)
        {
            markObject(vm->tempRoots[i]);
        }

        // Mark stack
        aupVal *top = vm->top +
            vm->frames[vm->frameCount - 1].function->locals;
        for (aupVal *slot = vm->stack; slot < top; slot++)
        {
            markValue(*slot);
        }

        // Mark call frames
        for (int i = 0; i < vm->frameCount; i++)
        {
            markObject((aupObj *)vm->frames[i].function);
        }

        // Mark upvalues
        for (aupUpv *upvalue = vm->openUpvals;
            upvalue != NULL;
            upvalue = upvalue->next)
        {
            markObject((aupObj *)upvalue);
        }
    }

    markTable(globals);
    //markCompilerRoots(vm);

    /* === Trace references === */
    while (m_gc.grayCount > 0)
    {
        aupObj *object = m_gc.grayStack[--m_gc.grayCount];
        switch (object->type) {
            case AUP_OSTR:
                break;
            case AUP_OUPV: {
                markValue(((aupUpv *)object)->closed);
                break;
            }
            case AUP_OFUN: {
                aupFun *function = (aupFun *)object;
                markObject((aupObj *)function->name);
                markArray(&function->chunk.constants);
                for (int i = 0; i < function->upvalCount; i++) {
                    markObject((aupObj *)function->upvals[i]);
                }
                break;
            }
            case AUP_OKLS: {
                aupKls *klass = (aupKls *)object;
                markObject((aupObj *)klass->name);
                break;
            }
            case AUP_OINC: {
                aupInc *instance = (aupInc *)object;
                markObject((aupObj *)instance->klass);
                markTable(&instance->fields);
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
                *object = m_gc.objects;
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
                m_gc.objects = object;
            }

            aup_freeObject(unreached);
        }
    }

    m_gc.nextGC = m_gc.allocated * 2;

    /* === Resume all threads === */
    /*
    for (aupVM *next = vm->next;
        next != vm;
        next = next->next)
    {
        // TODO
    }
    */
}
