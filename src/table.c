#include <stdlib.h>           
#include <string.h>           

#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD  0.75

void aup_initTable(aupTab *table)
{
    table->count = 0;
    table->space = 0;
    table->entries = NULL;
}

void aup_freeTable(aupTab *table)
{
    free(table->entries);
    aup_initTable(table);
}

static aupEnt *findEntry(aupEnt *entries, int space, aupStr *key)
{
    uint32_t index = key->hash % space;
    aupEnt *tombstone = NULL;

    for (;;) {
        aupEnt *entry = &entries[index];

        if (entry->key == NULL) {
            if (AUP_IsNil(entry->value)) {
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

        index = (index + 1) % space;
    }
}

bool aup_getKey(aupTab *table, aupStr *key, aupVal *value)
{
    if (table->count == 0) return false;

    aupEnt *entry = findEntry(table->entries, table->space, key);
    if (entry->key == NULL) {
        *value = AUP_VNil;
        return false;
    }

    *value = entry->value;
    return true;
}

static void expandTable(aupTab *table)
{
    int newSpace = AUP_GROW(table->space);
    aupEnt *entries = malloc(newSpace * sizeof(aupEnt));
    memset(entries, '\0', sizeof(aupEnt) * newSpace);

    /*
    for (int i = 0; i < newSpace; i++) {
        entries[i].key = NULL;
        entries[i].value = aup_vNil;
    }
    */

    table->count = 0;
    for (int i = 0; i < table->space; i++) {
        aupEnt *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        aupEnt *dest = findEntry(entries, newSpace, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        //*dest = *entry;
        //memcpy(dest, entry, sizeof(aupEnt));
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->space = newSpace;
}

bool aup_setKey(aupTab *table, aupStr *key, aupVal value)
{
    if (table->count >= (table->space * TABLE_MAX_LOAD)) {
        expandTable(table);
    }

    aupEnt *entry = findEntry(table->entries, table->space, key);

    bool isNewKey = entry->key == NULL;
    if (isNewKey && AUP_IsNil(entry->value)) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool aup_removeKey(aupTab *table, aupStr *key)
{
    if (table->count == 0) return false;

    // Find the entry.
    aupEnt *entry = findEntry(table->entries, table->space, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = AUP_VTrue;

    return true;
}

void aup_copyTable(aupTab *from, aupTab *to)
{
    for (int i = 0; i < from->space; i++) {
        aupEnt *entry = &from->entries[i];
        if (entry->key != NULL) {
            aup_setKey(to, entry->key, entry->value);
        }
    }
}

aupStr *aup_findString(aupTab *table, int length, uint32_t hash)
{
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->space;

    for (;;) {
        aupEnt *entry = &table->entries[index];
        aupStr *key = entry->key;

        if (key == NULL) {
            // Stop if we find an empty non-tombstone entry.                 
            if (AUP_IsNil(entry->value)) {
                return NULL;
            }
        }
        else if (key->length == length && key->hash == hash) {
            // We found it.
            return key;
        }

        index = (index + 1) % table->space;
    }
}
