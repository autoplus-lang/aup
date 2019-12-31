#include <stdlib.h>

#include "util.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC                                               
#include <stdio.h>                                                
#include "debug.h"                                                
#endif

#define GC_HEAP_GROW_FACTOR     2

void *aup_realloc(AUP_VM, void *ptr, size_t oldSize, size_t newSize)
{
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        aup_gc(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            aup_gc(vm);
        }
    }

	if (newSize == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newSize);
}

void aup_markObject(AUP_VM, aupO *object)
{
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC                 
    printf("%p mark ", (void*)object);
    aupV_print(AUP_OBJ(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = AUP_GROW_CAP(vm->grayCapacity);
        vm->grayStack = realloc(vm->grayStack,
            sizeof(aupO *) * vm->grayCapacity);
    }

    vm->grayStack[vm->grayCount++] = object;
}

void aup_markValue(AUP_VM, aupV value)
{
    if (!AUP_IS_OBJ(value)) return;
    aup_markObject(vm, AUP_AS_OBJ(value));
}

static void markArray(AUP_VM, aupVa *array)
{
    for (int i = 0; i < array->count; i++) {
        aup_markValue(vm, array->values[i]);
    }
}

static void blackenObject(AUP_VM, aupO *object)
{
#ifdef DEBUG_LOG_GC                     
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        //case OBJ_NATIVE:
        case AUP_TSTR:
            break;

        case AUP_TFUN: {
            aupOf *function = (aupOf *)object;
            aup_markObject(vm, (aupO *)function->name);
            markArray(vm, &function->chunk.constants);
            for (int i = 0; i < function->upvalueCount; i++) {
                aup_markObject(vm, (aupO *)function->upvalues[i]);
            }
            break;
        }

        case AUP_TUPV:
            aup_markValue(vm, ((aupOu *)object)->closed);
            break;
    }
}

static void freeObject(AUP_VM, aupO *object)
{
#ifdef DEBUG_LOG_GC                                        
    printf("%p free type %d\n", (void*)object, object->type);
#endif

	switch (object->type) {
		case AUP_TSTR: {
			aupOs *string = (aupOs*)object;
			free(string->chars);
			AUP_FREE(aupOs, object);
			break;
		}
		case AUP_TFUN: {
			aupOf *function = (aupOf*)object;
			aupCh_free(&function->chunk);
			free(function->upvalues);
			AUP_FREE(aupOf, object);
			break;
		}
		case AUP_TUPV: {
			AUP_FREE(aupOu, object);
			break;
		}
	}
}

static void markRoots(AUP_VM)
{
    for (aupV *slot = vm->stack; slot < vm->top; slot++) {
        aup_markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        aup_markObject(vm, (aupO *)vm->frames[i].function);
    }

    for (aupOu *upvalue = vm->openUpvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
        aup_markObject(vm, (aupO *)upvalue);
    }

    aup_markTable(vm, &vm->globals);
    aup_markCompilerRoots(vm);
}

static void traceReferences(AUP_VM)
{
    while (vm->grayCount > 0) {
        aupO *object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(AUP_VM)
{
    aupO *previous = NULL;
    aupO *object = vm->objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        }
        else {
            aupO *unreached = object;

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
#ifdef DEBUG_LOG_GC       
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots(vm);
    traceReferences(vm);
    aupT_removeWhite(&vm->strings);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC       
    printf("-- gc end\n");
    printf("   collected %ld bytes (from %ld to %ld) next at %ld\n",
        before - vm->bytesAllocated, before, vm->bytesAllocated,
        vm->nextGC);
#endif 
}

void aup_freeObjects(AUP_VM)
{
	aupO *object = vm->objects;
	while (object != NULL) {
		aupO *next = object->next;
		freeObject(vm, object);
		object = next;
	}

    free(vm->grayStack);
}
