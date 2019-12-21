#ifndef _AUP_TABLE_H
#define _AUP_TABLE_H
#pragma once

#include "value.h"

typedef struct {
	aupOs *key;
	aupV value;
} aupTe;

typedef struct {
	int count;
	int capacity;
	aupTe *entries;
} aupT;

void aupT_init(aupT *table);

#endif
