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
void aupT_free(aupT *table);
bool aupT_get(aupT *table, aupOs *key, aupV *value);
bool aupT_set(aupT *table, aupOs *key, aupV value);
void aupT_addAll(aupT *from, aupT *to);

#endif
