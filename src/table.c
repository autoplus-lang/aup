#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "memory.h"

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