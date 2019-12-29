#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "vm.h"
#include "context.h"

#define TYPE(o)		((o)->type)
#define AS_STR(o)	((aupOs *)(o))
#define AS_CSTR(o)	(((aupOs *)(o))->chars)
#define AS_FUN(o)	((aupOf *)(o))

static const char
	__obj[] = "obj",
	__str[] = "str",
	__fun[] = "fn";

const char *aupO_typeOf(aupO *object)
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

static void printFunction(aupOf *function)
{
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("%s: <%s>", __fun, function->name->chars);
}

void aupO_print(aupO *object)
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

static aupO *allocObject(AUP_VM, size_t size, aupVt type)
{
	aupO *object = (aupO *)aup_realloc(vm, NULL, 0, size);
	object->type = type;

	object->next = vm->objects;
	vm->objects = object;
	return object;
}

static aupOs *allocString(AUP_VM, char *chars, int length, uint32_t hash)
{
	aupOs *string = ALLOC_OBJ(aupOs, AUP_TSTR);
	string->length = length;
	string->chars = chars;
	string->hash = hash;

	aupT_set(&vm->ctx->strings, string, AUP_NIL);

	return string;
}

aupOs *aupOs_take(AUP_VM, char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOs *interned = aupT_findString(&vm->ctx->strings, chars, length, hash);
	if (interned != NULL) {
		free(chars);
		return interned;
	}

	return allocString(vm, chars, length, hash);
}

aupOs *aupOs_copy(AUP_VM, const char *chars, int length)
{
	uint32_t hash = hashString(chars, length);

	aupOs *interned = aupT_findString(&vm->ctx->strings, chars, length, hash);
	if (interned != NULL) return interned;

	char *heapChars = malloc(length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return allocString(vm, heapChars, length, hash);
}

aupOf *aupOf_new(AUP_VM)
{
	aupOf *function = ALLOC_OBJ(aupOf, AUP_TFUN);

	function->arity = 0;
	function->upvalueCount = 0;
	function->upvalues = NULL;
	function->name = NULL;
	aupCh_init(&function->chunk);

	return function;
}

void aupOf_closure(aupOf *function)
{
	int count = function->upvalueCount;
	aupOu **upvalues = malloc(count * sizeof(aupOu *));

	for (int i = 0; i < count; i++) {
		upvalues[i] = NULL;
	}

	function->upvalues = upvalues;
}

aupOu *aupOu_new(AUP_VM, aupV *slot)
{
	aupOu *upvalue = ALLOC_OBJ(aupOu, AUP_TUPV);
	upvalue->value = slot;
	upvalue->next = NULL;

	return upvalue;
}
