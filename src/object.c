#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"

#define ALLOC_OBJ(type, objectType) \
	(type *)allocObject(sizeof(type), objectType)

static aupO *allocObject(size_t size, aupVt type)
{
	aupO *object = (aupO *)aup_realloc(NULL, 0, size);
	object->type = type;
	return object;
}

static aupOs *allocString(char *chars, int length)
{
	aupOs *string = ALLOC_OBJ(aupOs, AUP_TSTR);
	string->length = length;
	string->chars = chars;

	return string;
}

aupOs *aupOs_take(char *chars, int length)
{
	return allocString(chars, length);
}

aupOs *aupOs_copy(const char *chars, int length)
{
	char *heapChars = AUP_ALLOC(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return allocString(heapChars, length);
}
