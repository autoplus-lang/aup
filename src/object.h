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
	char *src;
	int len;
	uint32_t hash;
};

#endif
