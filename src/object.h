#ifndef _AUP_OBJECT_H
#define _AUP_OBJECT_H
#pragma once

#include "value.h"

struct _aupO {
	aupVt type;
	struct _aupO *next;
};

struct _aupOs {
	struct _aupO obj;
	char *chars;
	int length;
	uint32_t hash;
};

aupOs *aupOs_take(AUP_VM, char *chars, int length);
aupOs *aupOs_copy(AUP_VM, const char *chars, int length);

void aupO_print(aupO *object);

#endif
