#include <stdlib.h>

#include "util.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

void *aup_realloc(AUP_VM, void *ptr, size_t oldSize, size_t newSize)
{
	if (newSize == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newSize);
}

static void freeObject(AUP_VM, aupO *object)
{
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

void aup_freeObjects(AUP_VM)
{
	aupO *object = vm->objects;
	while (object != NULL) {
		aupO* next = object->next;
		freeObject(vm, object);
		object = next;
	}
}
