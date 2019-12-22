#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"

#define TYPE(o)		((o)->type)
#define AS_STR(o)	((aupOs *)(o))
#define AS_CSTR(o)	(((aupOs *)(o))->chars)

void aupO_print(aupO *object)
{
	switch (TYPE(object)) {
		case AUP_TSTR: {
			printf("%s", AS_CSTR(object));
			break;
		}
	}
}

static uint32_t hashString(const char *key, int length)
{
	// fnv_32
	static const int prime = 16777619;
	static const uint32_t basis = 2166136261U;

	uint32_t hash = basis;

	if (length < 0) while (*key != '\0') {
		hash ^= *(key++);
		hash *= prime;
	}
	else for (int i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= prime;
	}

	return hash;
}

#define ALLOC_OBJ(vm, type, objectType) \
	(type *)allocObject(vm, sizeof(type), objectType)

static aupO *allocObject(AUP_VM, size_t size, aupVt type)
{
	aupO *object = (aupO *)aup_realloc(NULL, 0, size);
	object->type = type;

	object->next = vm->objects;
	vm->objects = object;
	return object;
}

static aupOs *allocString(AUP_VM, char *chars, int length, uint32_t hash)
{
	aupOs *string = ALLOC_OBJ(vm, aupOs, AUP_TSTR);
	string->length = length;
	string->chars = chars;
	string->hash = hash;

	aupT_set(&vm->strings, string, AUP_NIL);

	return string;
}

aupOs *aupOs_take(AUP_VM, char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOs *interned = aupT_findString(&vm->strings, chars, length, hash);
	if (interned != NULL) {
		AUP_FREE_ARR(char, chars, length + 1);
		return interned;
	}

	return allocString(vm, chars, length, hash);
}

aupOs *aupOs_copy(AUP_VM, const char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOs *interned = aupT_findString(&vm->strings, chars, length, hash);
	if (interned != NULL) return interned;

	char *heapChars = AUP_ALLOC(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return allocString(vm, heapChars, length, hash);
}
