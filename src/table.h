#ifndef _AUP_TABLE_H
#define _AUP_TABLE_H
#pragma once

#include "common.h"
#include "value.h"

typedef struct _aupEnt aupEnt;
typedef struct _aupIdx aupIdx;

typedef struct {
    int count;
    int capacity;
    aupEnt *entries;
} aupTab;

typedef struct {
    int count;
    int capacity;
    aupIdx *indexes;
} aupHash;

void aup_initTable(aupTab *table);
void aup_freeTable(aupTab *table);

bool aup_getTable(aupTab *table, aupStr *key, aupVal *value);
bool aup_setTable(aupTab *table, aupStr *key, aupVal value);
bool aup_tableRemove(aupTab *table, aupStr *key);
void aup_addTable(aupTab *from, aupTab *to);

aupStr *aup_findString(aupTab *table, const char *chars, int length, uint32_t hash);

void aup_initHash(aupHash *hash);
void aup_freeHash(aupHash *hash);

bool aup_getHash(aupHash *hash, uint64_t key, aupVal *value);
bool aup_setHash(aupHash *hash, uint64_t key, aupVal value);

void aup_tableRemoveWhite(aupTab *table);
void aup_markTable(aupVM *vm, aupTab *table);
void aup_markHash(aupVM *vm, aupHash *hash);

#endif
