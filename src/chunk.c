#include <stdio.h>

#include "chunk.h"
#include "memory.h"

void aupCh_init(aupCh *chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	chunk->columns = NULL;

	aupVa_init(&chunk->constants);
}

int aupCh_write(aupCh *chunk, uint32_t instruction, uint16_t line, uint16_t column)
{
	if (chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		chunk->capacity = AUP_GROW_CAP(oldCapacity);
		chunk->code = AUP_GROW_ARR(uint32_t, chunk->code, oldCapacity, chunk->capacity);
		chunk->lines = AUP_GROW_ARR(uint16_t, chunk->lines, oldCapacity, chunk->capacity);
		chunk->columns = AUP_GROW_ARR(uint16_t, chunk->columns, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = instruction;
	chunk->lines[chunk->count] = line;
	chunk->columns[chunk->count] = column;
	return chunk->count++;
}

void aupCh_free(aupCh *chunk)
{
	AUP_FREE_ARR(uint32_t, chunk->code, chunk->capacity);
	aupVa_free(&chunk->constants);

	aupCh_init(chunk);
}

int aupCh_addConst(aupCh *chunk, aupV value)
{
	aupVa_write(&chunk->constants, value);
}
