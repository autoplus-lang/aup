#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"

#define TYPE(o)		((o)->type)
#define AS_STR(o)	((aupOstr *)(o))
#define AS_CSTR(o)	(((aupOstr *)(o))->chars)
#define AS_FUN(o)	((aupOfun *)(o))

static const char
	__obj[] = "obj",
	__str[] = "str",
	__fun[] = "fn";

const char *aup_typeofObj(aupObj *object)
{
	switch (TYPE(object)) {
		case AUP_TSTR:
			return __str;
		case AUP_TFUN:
			return __fun;
		default:
			return __obj;
	}
}

static void printFunction(aupOfun *function)
{
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("%s: <%s>", __fun, function->name->chars);
}

void aup_printObj(aupObj *object)
{
	switch (TYPE(object)) {
		case AUP_TSTR: {
			printf("%s", AS_CSTR(object));
			break;
		}
		case AUP_TFUN: {
			printFunction(AS_FUN(object));
			break;
		}
		default: {
			printf("%s: %p", __obj, object);
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

#define ALLOC_OBJ(type, objectType) \
	(type *)allocObject(vm, sizeof(type), objectType)

static aupObj *allocObject(AUP_VM, size_t size, aupVt type)
{
    aupObj *object = (aupObj *)aup_realloc(vm, NULL, 0, size);
	object->type = type;
    object->isMarked = false;

	object->next = vm->objects;
	vm->objects = object;

#ifdef AUP_DEBUG
    printf("%p allocate %ld for %d\n", (void*)object, size, type);
#endif
	return object;
}

static aupOstr *allocString(AUP_VM, char *chars, int length, uint32_t hash)
{
	aupOstr *string = ALLOC_OBJ(aupOstr, AUP_TSTR);
	string->length = length;
	string->chars = chars;
	string->hash = hash;

    aup_pushRoot(vm, (aupObj *)string);
	aupT_set(&vm->strings, string, AUP_NIL);
    aup_popRoot(vm);

	return string;
}

aupOstr *aup_takeString(AUP_VM, char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOstr *interned = aupT_findString(&vm->strings, chars, length, hash);
	if (interned != NULL) {
		free(chars);
		return interned;
	}

	return allocString(vm, chars, length, hash);
}

aupOstr *aup_copyString(AUP_VM, const char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOstr *interned = aupT_findString(&vm->strings, chars, length, hash);
	if (interned != NULL) return interned;

	char *heapChars = malloc(length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return allocString(vm, heapChars, length, hash);
}

aupOfun *aup_newFunction(AUP_VM)
{
	aupOfun *function = ALLOC_OBJ(aupOfun, AUP_TFUN);

	function->arity = 0;
	function->upvalueCount = 0;
	function->upvalues = NULL;
	function->name = NULL;
	aup_initChunk(&function->chunk);

	return function;
}

void aup_makeClosure(aupOfun *function)
{
	int count = function->upvalueCount;
	aupOupv **upvalues = malloc(count * sizeof(aupOupv *));

	for (int i = 0; i < count; i++) {
		upvalues[i] = NULL;
	}

	function->upvalues = upvalues;
}

aupOupv *aup_newUpval(AUP_VM, aupVal *slot)
{
    aupOupv *upvalue = ALLOC_OBJ(aupOupv, AUP_TUPV);
	upvalue->value = slot;
	upvalue->next = NULL;

	return upvalue;
}
