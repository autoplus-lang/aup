#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "object.h"
#include "memory.h"

#define TABLE_MAX_LOAD	0.75

void aupT_init(aupT *table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void aupT_free(aupT *table)
{
	AUP_FREE_ARR(aupTe, table->entries, table->capacity);
	aupT_init(table);
}

static aupTe *findEntry(aupTe *entries, int capacity, aupOs *key)
{
	uint32_t index = key->hash % capacity;
	for (;;) {
		aupTe *entry = &entries[index];

		if (entry->key == key || entry->key == NULL) {
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

static void adjustCapacity(aupT *table, int capacity)
{
	aupTe *entries = AUP_ALLOC(aupTe, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = AUP_NIL;
	}

	for (int i = 0; i < table->capacity; i++) {
		aupTe *entry = &table->entries[i];
		if (entry->key == NULL) continue;

		aupTe* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
	}

	AUP_FREE_ARR(aupTe, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool aupT_set(aupT *table, aupOs *key, aupV value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = AUP_GROW_CAP(table->capacity);
		adjustCapacity(table, capacity);
	}

	aupTe *entry = findEntry(table->entries, table->capacity, key);

	bool isNewKey = entry->key == NULL;
	if (isNewKey) table->count++;

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

void aupT_addAll(aupT *from, aupT *to)
{
	for (int i = 0; i < from->capacity; i++) {
		aupTe *entry = &from->entries[i];
		if (entry->key != NULL) {
			aupT_set(to, entry->key, entry->value);
		}
	}
}
