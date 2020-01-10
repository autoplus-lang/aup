#include <stdlib.h>

#include "table.h"
#include "value.h"
#include "object.h"
#include "gc.h"

struct _aupEnt {
    aupStr *key;
    aupVal value;
};

struct _aupIdx {
    uint64_t key;
    aupVal value;
};

#define MAX_LOAD    0.75
#define UNUSED_KEY  ((uint64_t)(-1))

void aup_initTable(aupTab *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void aup_freeTable(aupTab *table)
{
    free(table->entries);
    aup_initTable(table);
}

static aupEnt *findEntry(aupEnt *entries, int capacity, aupStr *key)
{
    uint32_t index = key->hash % capacity;
    aupEnt *tombstone = NULL;

    for (;;) {
        aupEnt *entry = &entries[index];

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

bool aup_getTable(aupTab *table, aupStr *key, aupVal *value)
{
    if (table->count == 0) return false;

    aupEnt *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

static void growTable(aupTab *table, int capacity)
{
    aupEnt *entries = malloc(capacity * sizeof(aupEnt));

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = AUP_NIL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        aupEnt *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        aupEnt *dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

bool aup_setTable(aupTab *table, aupStr *key, aupVal value)
{
    if (table->count + 1 > table->capacity * MAX_LOAD) {
        int capacity = AUP_GROWCAP(table->capacity);
        growTable(table, capacity);
    }

    aupEnt *entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == NULL;
    if (isNewKey && AUP_IS_NIL(entry->value)) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool aup_tableRemove(aupTab *table, aupStr *key)
{
    if (table->count == 0) return false;

    // Find the entry.                                             
    aupEnt *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.                             
    entry->key = NULL;
    entry->value = AUP_TRUE;

    return true;
}

void aup_addTable(aupTab *from, aupTab *to)
{
    for (int i = 0; i < from->capacity; i++) {
        aupEnt *entry = &from->entries[i];
        if (entry->key != NULL) {
            aup_setTable(to, entry->key, entry->value);
        }
    }
}

aupStr *aup_findString(aupTab *table, const char *chars, int length, uint32_t hash)
{
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    for (;;) {
        aupEnt *entry = &table->entries[index];
        aupStr *key = entry->key;

        if (key == NULL) {
            // Stop if we find an empty non-tombstone entry.                 
            if (AUP_IS_NIL(entry->value)) return NULL;
        }
        else if (key->length == length && key->hash == hash) {
            // We found it.                                                  
            return key;
        }

        index = (index + 1) % table->capacity;
    }
}

void aup_initHash(aupHash *hash)
{
    hash->count = 0;
    hash->capacity = 0;
    hash->indexes = NULL;
}

void aup_freeHash(aupHash *hash)
{
    free(hash->indexes);
    aup_initHash(hash);
}

static aupIdx *findIndex(aupIdx *indexes, int capacity, uint64_t key)
{
    uint32_t i = key % capacity;
    aupIdx *tombstone = NULL;

    for (;;) {
        aupIdx *index = &indexes[i];

        if (index->key == UNUSED_KEY) {
            if (AUP_IS_NIL(index->value)) {
                // Empty entry.                              
                return tombstone != NULL ? tombstone : index;
            }
            else {
                // We found a tombstone.                     
                if (tombstone == NULL) tombstone = index;
            }
        }
        else if (index->key == key) {
            // We found the key.
            return index;
        }

        i = (i + 1) % capacity;
    }
}

static void growHash(aupHash *hash, int capacity)
{
    aupIdx *indexes = malloc(capacity * sizeof(aupIdx));

    for (int i = 0; i < capacity; i++) {
        indexes[i].key = UNUSED_KEY;
        indexes[i].value = AUP_NIL;
    }

    hash->count = 0;
    for (int i = 0; i < hash->capacity; i++) {
        aupIdx *index = &hash->indexes[i];
        if (index->key == UNUSED_KEY) continue;

        aupIdx *dest = findIndex(indexes, capacity, index->key);
        dest->key = index->key;
        dest->value = index->value;
        hash->count++;
    }

    free(hash->indexes);
    hash->indexes = indexes;
    hash->capacity = capacity;
}

bool aup_getHash(aupHash *hash, uint64_t key, aupVal *value)
{
    if (hash->count == 0) return false;

    aupIdx *index = findIndex(hash->indexes, hash->capacity, key);
    if (index->key == UNUSED_KEY) {
        return false;
    }

    (*value) = index->value;
    return true;
}

bool aup_setHash(aupHash *hash, uint64_t key, aupVal value)
{
    if (hash->count + 1 > hash->capacity * MAX_LOAD) {
        int capacity = AUP_GROWCAP(hash->capacity);
        growHash(hash, capacity);
    }

    aupIdx *index = findIndex(hash->indexes, hash->capacity, key);

    bool isNewKey = (index->key == UNUSED_KEY);
    if (isNewKey && AUP_IS_NIL(index->value)) {
        hash->count++;
    }

    index->key = key;
    index->value = value;
    return isNewKey;
}

void aup_tableRemoveWhite(aupTab *table)
{
    for (int i = 0; i < table->capacity; i++) {
        aupEnt *entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            aup_tableRemove(table, entry->key);
        }
    }
}

void aup_markTable(aupVM *vm, aupTab *table)
{
    for (int i = 0; i < table->capacity; i++) {
        aupEnt *entry = &table->entries[i];
        aup_markObject(vm, (aupObj *)entry->key);
        aup_markValue(vm, entry->value);
    }
}

void aup_markHash(aupVM *vm, aupHash *hash)
{
    for (int i = 0; i < hash->capacity; i++) {
        aupIdx *index = &hash->indexes[i];
        aup_markValue(vm, index->value);
    }
}
