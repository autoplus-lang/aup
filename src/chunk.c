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

int aupCh_addK(aupCh *chunk, aupV value)
{
	aupVa_write(&chunk->constants, value);
}

void aupCh_dasmInst(aupCh *chunk, int offset)
{
	uint32_t i;
	printf("%03d ", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("  | ");
	}
	else {
		printf("%3d:", chunk->lines[offset]);
	}

	if (offset > 0 && (chunk->columns[offset] == chunk->columns[offset - 1])
		&& (chunk->lines[offset] == chunk->lines[offset - 1])) {
		printf("|    ");
	}
	else {
		printf("%-3d  ", chunk->columns[offset]);
	}

#define GET_Op()	AUP_GET_Op(i)
#define GET_A()		AUP_GET_A(i)
#define GET_Ax()	AUP_GET_Ax(i)
#define GET_B()		AUP_GET_B(i)
#define GET_C()		AUP_GET_C(i)
#define GET_sB()	AUP_GET_sB(i)
#define GET_sC()	AUP_GET_sC(i)

#define GET_Bx()	AUP_GET_Bx(i)
#define GET_Cx()	AUP_GET_Cx(i)

#define REG_K(i)	(chunk->constants.values[i])

#define dispatch()  switch (GET_Op())
#define code(x)		case (AUP_OP_##x): printf("%-5s ", #x);
#define code_err()  default:
#define next		break

	i = chunk->code[i];
	printf("%02x %2x %3x %3x  ", GET_Op(), GET_A(), GET_Bx(), GET_Cx());

	dispatch() {
		code(NOP)
			next;
		code_err()
			printf("bad opcode, got %d", GET_Op());
			break;
	}

	printf("\n");
}

void aupCh_dasm(aupCh *chunk, const char *name)
{
	printf("\n");

	printf("off ln  col  op  A  Bx  Cx  dasm\n");
	printf("--- --- ---  -------------  --------------------\n");

	for (int offset = 0; offset < chunk->count; offset++) {
		aupCh_dasmInst(chunk, offset);
	}

	printf("===> %s\n", name);
}