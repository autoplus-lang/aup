#include <stdlib.h>

#include "util.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

void *aup_realloc(void *ptr, size_t oldSize, size_t newSize)
{
	if (newSize == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newSize);
}

static void freeObject(aupO *object)
{
	switch (object->type) {
		case AUP_TSTR: {
			aupOs *string = (aupOs*)object;
			AUP_FREE_ARR(char, string->chars, string->length + 1);
			AUP_FREE(aupOs, object);
			break;
		}
		case AUP_TFUN: {
			aupOf *function = (aupOf*)object;
			aupCh_free(&function->chunk);
			AUP_FREE(aupOf, object);
			break;
		}
	}
}

void aup_freeObjects(AUP_VM)
{
	aupO *object = vm->objects;
	while (object != NULL) {
		aupO* next = object->next;
		freeObject(object);
		object = next;
	}
}