#include <stdlib.h>

#include "util.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

#ifdef AUP_DEBUG
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR     2

void *aup_defaultAlloc(void *ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, size);
}

void *aup_realloc(AUP_VM, void *ptr, size_t oldSize, size_t newSize)
{
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef AUP_DEBUG
        aup_gc(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            aup_gc(vm);
        }
    }

    return vm->alloc(ptr, newSize);
}

void aup_markObject(AUP_VM, aupObj *object)
{
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef AUP_DEBUG
    printf("%p mark ", (void*)object);
    aup_printVal(AUP_OBJ(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = AUP_GROW_CAP(vm->grayCapacity);
        vm->grayStack = realloc(vm->grayStack,
            sizeof(aupObj *) * vm->grayCapacity);
    }

    vm->grayStack[vm->grayCount++] = object;
}

void aup_markValue(AUP_VM, aupVal value)
{
    if (!AUP_IS_OBJ(value)) return;
    aup_markObject(vm, AUP_AS_OBJ(value));
}

static void markArray(AUP_VM, aupValArr *array)
{
    for (int i = 0; i < array->count; i++) {
        aup_markValue(vm, array->values[i]);
    }
}

static void blackenObject(AUP_VM, aupObj *object)
{
#ifdef AUP_DEBUG
    printf("%p blacken ", (void*)object);
    aup_printVal(AUP_OBJ(object));
    printf("\n");
#endif

    switch (object->type) {
        //case OBJ_NATIVE:
        case AUP_TSTR:
            break;

        case AUP_TFUN: {
            aupOfun *function = (aupOfun *)object;
            aup_markObject(vm, (aupObj *)function->name);
            markArray(vm, &function->chunk.constants);
            for (int i = 0; i < function->upvalueCount; i++) {
                aup_markObject(vm, (aupObj *)function->upvalues[i]);
            }
            break;
        }

        case AUP_TUPV:
            aup_markValue(vm, ((aupOupv *)object)->closed);
            break;
    }
}

static void freeObject(AUP_VM, aupObj *object)
{
#ifdef AUP_DEBUG
    printf("%p free type %d\n", (void*)object, object->type);
#endif

	switch (object->type) {
		case AUP_TSTR: {
			aupOstr *string = (aupOstr *)object;
			free(string->chars);
			AUP_FREE(aupOstr, object);
			break;
		}
		case AUP_TFUN: {
			aupOfun *function = (aupOfun *)object;
			aup_freeChunk(&function->chunk);
			free(function->upvalues);
			AUP_FREE(aupOfun, object);
			break;
		}
		case AUP_TUPV: {
			AUP_FREE(aupOupv, object);
			break;
		}
	}
}

static void markRoots(AUP_VM)
{
    for (int i = 0; i < vm->numTempRoots; i++) {
        aup_markObject(vm, vm->tempRoots[i]);
    }

    for (aupVal *slot = vm->stack; slot < vm->top; slot++) {
        aup_markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        aup_markObject(vm, (aupObj *)vm->frames[i].function);
    }

    for (aupOupv *upvalue = vm->openUpvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
        aup_markObject(vm, (aupObj *)upvalue);
    }

    aup_markTable(vm, &vm->globals);
    aup_markCompilerRoots(vm);
}

static void traceReferences(AUP_VM)
{
    while (vm->grayCount > 0) {
        aupObj *object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(AUP_VM)
{
    aupObj *previous = NULL;
    aupObj *object = vm->objects;

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
                vm->objects = object;
            }

            freeObject(vm, unreached);
        }
    }
}

void aup_gc(AUP_VM)
{
#ifdef AUP_DEBUG
    printf("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif

    markRoots(vm);
    traceReferences(vm);
    aupT_removeWhite(&vm->strings);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef AUP_DEBUG
    printf("-- gc end\n");
    printf("   collected %ld bytes (from %ld to %ld) next at %ld\n",
        before - vm->bytesAllocated, before, vm->bytesAllocated,
        vm->nextGC);
#endif 
}

void aup_freeObjects(AUP_VM)
{
    aupObj *object = vm->objects;
	while (object != NULL) {
        aupObj *next = object->next;
		freeObject(vm, object);
		object = next;
	}

    free(vm->grayStack);
}
