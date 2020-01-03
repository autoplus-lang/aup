#ifndef _AUP_CHUNK_H
#define _AUP_CHUNK_H
#pragma once

#include "value.h"
#include "opcode.h"

typedef struct {
	int count;
	int capacity;
	uint32_t *code;
	uint16_t *lines;
	uint16_t *columns;
	aupVa constants;
} aupChunk;

#define AUP_CODE_PAGE	256

void aup_initChunk(aupChunk *chunk);
void aup_freeChunk(aupChunk *chunk);
int aup_writeChunk(aupChunk *chunk, uint32_t instruction, uint16_t line, uint16_t column);
int aup_addConstant(aupChunk *chunk, aupV value);

void aup_dasmChunk(aupChunk *chunk, const char *name);
void aup_dasmInstruction(aupChunk *chunk, int offset);

#endif
