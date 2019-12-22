#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"

void aupCh_init(aupCh *chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	chunk->columns = NULL;

	aupVa_init(&chunk->constants);
}

void aupCh_free(aupCh *chunk)
{
	free(chunk->code);
	free(chunk->lines);
	free(chunk->columns);
	aupVa_free(&chunk->constants);

	aupCh_init(chunk);
}

int aupCh_write(aupCh *chunk, uint32_t instruction, uint16_t line, uint16_t column)
{
	if (chunk->capacity < chunk->count + 1) {
		int cap = (chunk->capacity += AUP_CODE_PAGE);
		chunk->code = realloc(chunk->code, cap * sizeof(uint32_t));
		chunk->lines = realloc(chunk->lines, cap * sizeof(uint16_t));
		chunk->columns = realloc(chunk->columns, cap * sizeof(uint16_t));
	}

	chunk->code[chunk->count] = instruction;
	chunk->lines[chunk->count] = line;
	chunk->columns[chunk->count] = column;
	return chunk->count++;
}

int aupCh_addK(aupCh *chunk, aupV value)
{
	return aupVa_write(&chunk->constants, value);
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

	i = chunk->code[offset];
	printf("%02x %2x %3x %3x  ", GET_Op(), GET_A(), GET_Bx(), GET_Cx());

	dispatch() {
		code(NOP) {
			next;
		}

		code(RET) {
			if (GET_sB())
				printf("R[%d]", GET_A());
			else
				printf("nil");
			next;
		}
		code(CALL) {
			int A = GET_A(), argc = GET_B();
			printf("R[%d] = R[%d](", A, A);
			if (argc > 0) {
				for (int i = 1; i <= argc; i++) {
					printf("R[%d]", A + i);
					if (i < argc) printf(", ");
				}
			}
			printf(")");
			next;
		}

		code(MOV) {
			printf("R[%d] = R[%d]", GET_A(), GET_B());
			next;
		}

		code(PUT) {
			if (GET_B() > 1) {
				printf("R[%d]..R[%d]", GET_A(), GET_A() + GET_B() - 1);		
			}
			else {
				printf("R[%d]", GET_A());
			}
			next;
		}

		code(NIL) {
			printf("R[%d] = nil", GET_A());
			next;
		}
		code(BOL) {
			printf("R[%d] = %s", GET_A(), GET_sB() ? "true" : "false");
			next;
		}
		code(LDK) {
			printf("R[%d] = K[%d]", GET_A(), GET_B());
			next;
		}

		code(NOT) {
			printf("R[%d] = !R[%d]", GET_A(), GET_B());
			next;
		}
		code(NEG) {
			printf("R[%d] = -R[%d]", GET_A(), GET_B());
			next;
		}

		code(LT) {
			printf("R[%d] = R[%d] < R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(LE) {
			printf("R[%d] = R[%d] <= R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(EQ) {
			printf("R[%d] = R[%d] == R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}

		code(ADD) {
			printf("R[%d] = R[%d] + R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(SUB) {
			printf("R[%d] = R[%d] - R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(MUL) {
			printf("R[%d] = R[%d] * R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(DIV) {
			printf("R[%d] = R[%d] / R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}
		code(MOD) {
			printf("R[%d] = R[%d] %% R[%d]", GET_A(), GET_B(), GET_C());
			next;
		}

		code(DEF) {
			printf("G.K[%d] = ", GET_A());
			if (GET_sB()) printf("nil");
			else printf("R[%d]", GET_B());
			next;
		}
		code(GLD) {
			printf("R[%d] = G.K[%d]", GET_A(), GET_B());
			next;
		}
		code(GST) {
			printf("G.K[%d] = R[%d]", GET_A(), GET_B());
			next;
		}

		code(LD) {
			printf("R[%d] = R[%d]", GET_A(), GET_B());
			next;
		}
		code(ST) {
			printf("R[%d] = R[%d]", GET_A(), GET_B());
			next;
		}

		code(JMP) {
			printf("ip -> %03d", offset + GET_Ax() + 1);
			next;
		}
		code(JMPF) {
			printf("ip -> %03d, if !R[%d]", offset + GET_Ax() + 1, GET_C());
			next;
		}

		code_err() {
			printf("bad opcode, got %d", GET_Op());
			next;
		}
	}

	printf("\n");
}

void aupCh_dasm(aupCh *chunk, const char *name)
{
	printf(">> disassembling chunk: <%s>\n", name);

	for (int i = 0; i < chunk->constants.count; i++) {
		printf("K[%d] = ", i); aupV_print(chunk->constants.values[i]);
		printf("\n");
	}

	printf("------------------------------------------------\n");

	printf("off  ln col  op  A  Bx  Cx  description\n");
	printf("--- --- ---  -------------  --------------------\n");

	for (int offset = 0; offset < chunk->count; offset++) {
		aupCh_dasmInst(chunk, offset);
	}

	printf("------------------------------------------------\n\n");
}