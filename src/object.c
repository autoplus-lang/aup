#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"

static uint32_t hashString(const char *key, int length)
{
	uint32_t hash = 2166136261u;

	for (int i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= 16777619;
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

	return string;
}

aupOs *aupOs_take(AUP_VM, char *chars, int length)
{
	uint32_t hash = hashString(chars, length);
	return allocString(vm, chars, length, hash);
}

aupOs *aupOs_copy(AUP_VM, const char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	char *heapChars = AUP_ALLOC(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return allocString(vm, heapChars, length, hash);
}
