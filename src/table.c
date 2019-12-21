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
	aupTe *tombstone = NULL;

	for (;;) {
		aupTe *entry = &entries[index];

		if (entry->key == NULL) {
			if (AUP_IS_NIL(entry->value)) {
				// Empty entry.                              
				return tombstone != NULL ? tombstone : entry;
			}
			else {
				// We found a tombstone.                     
				if (tombstone == NULL) tombstone = entry;
			}
		}
		else if (entry->key == key) {
			// We found the key.                           
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

bool aupT_get(aupT *table, aupOs *key, aupV *value)
{
	if (table->count == 0) return false;

	aupTe *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	*value = entry->value;
	return true;
}

static void adjustCapacity(aupT *table, int capacity)
{
	aupTe *entries = AUP_ALLOC(aupTe, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = AUP_NIL;
	}

	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		aupTe *entry = &table->entries[i];
		if (entry->key == NULL) continue;

		aupTe* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;

		table->count++;
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
	if (isNewKey && AUP_IS_NIL(entry->value)) table->count++;

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool aupT_delete(aupT *table, aupOs *key)
{
	if (table->count == 0) return false;

	// Find the entry.                                             
	aupTe *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// Place a tombstone in the entry.                             
	entry->key = NULL;
	entry->value = AUP_BOOL(true);

	return true;
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

aupOs *aupT_findString(aupT *table, const char *chars, int length, uint32_t hash)
{
	if (table->count == 0) return NULL;

	uint32_t index = hash % table->capacity;

	for (;;) {
		aupTe *entry = &table->entries[index];

		if (entry->key == NULL) {
			// Stop if we find an empty non-tombstone entry.                 
			if (AUP_IS_NIL(entry->value)) return NULL;
		}
		else if (entry->key->length == length &&
			entry->key->hash == hash &&
			memcmp(entry->key->chars, chars, length) == 0) {
			// We found it.                                                  
			return entry->key;
		}

		index = (index + 1) % table->capacity;
	}
}