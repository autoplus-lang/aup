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
} aupCh;

#define AUP_CODE_PAGE	256

void aupCh_init(aupCh *chunk);
void aupCh_free(aupCh *chunk);
int aupCh_write(aupCh *chunk, uint32_t instruction, uint16_t line, uint16_t column);
int aupCh_addK(aupCh *chunk, aupV value);

void aupCh_dasm(aupCh *chunk, const char *name);
void aupCh_dasmInst(aupCh *chunk, int offset);

#endif
