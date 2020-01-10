#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "object.h"
#include "vm.h"

void aup_initGC(aupGC *gc)
{
    gc->nextGC = 0;
    gc->allocated = 0;
    gc->objects = NULL;
}

void aup_freeGC(aupGC *gc)
{
    aupObj *object = gc->objects;

    while (object != NULL) {
        aupObj *next = object->next;
        aup_freeObject(gc, object);
        object = next;
    }
}

void *aup_realloc(aupGC *gc, void *ptr, size_t old, size_t new)
{
    gc->allocated += new - old;

    if (new > old && gc->allocated > gc->nextGC) {
        // todo
        // aup_collect;
    }

    if (new == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, new);
}

void aup_collect(aupVM *vm)
{

}
