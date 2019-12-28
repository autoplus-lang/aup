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
	int k = aupVa_find(&chunk->constants, value);
	if (k != -1) return k;

	return aupVa_write(&chunk->constants, value);
}

void aupCh_dasmInst(aupCh *chunk, int offset)
{
	uint32_t i;
	printf("%03d.", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("  | ");
	}
	else {
		printf("%3d:", chunk->lines[offset]);
	}

	if (offset > 0 && (chunk->columns[offset] == chunk->columns[offset - 1])
		&& (chunk->lines[offset] == chunk->lines[offset - 1])) {
		printf("|   ");
	}
	else {
		printf("%-3d ", chunk->columns[offset]);
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

#define R(i)		printf("R[%d]", i)
#define K(i)		printf("K[%d]", i)

#define R_A()		R(GET_A())
#define R_B()		R(GET_B())
#define R_C()		R(GET_C())

#define K_A()		K(GET_A())
#define K_B()		K(GET_B())
#define K_C()		K(GET_C())

#define RK_B()		GET_sB() ? K_B() : R_B()
#define RK_C()		GET_sC() ? K_C() : R_C()

#define PUT(x)      printf(x)
#define PUTF(f,...) printf(f, ##__VA_ARGS__)

#define DISPATCH()  switch (GET_Op())
#define CODE(x)		case (AUP_OP_##x): printf("%-5s ", #x);
#define CODE_ERR()  default:
#define NEXT		break

	i = chunk->code[offset];
	printf("[%02x %2x %3x %3x] -> ", GET_Op(), GET_A(), GET_Bx(), GET_Cx());

    DISPATCH()
    {
		CODE(NOP)
        {
			NEXT;
		}

		CODE(RET)
        {
			GET_A() ? RK_B() : PUT("nil");
			NEXT;
		}

		CODE(CALL)
        {
			int A = GET_A(), argc = GET_B();
            PUTF("R[%d] = R[%d](", A, A);
		    for (int i = 1; i <= argc; i++) {
                PUTF("R[%d]", A + i);
                if (i < argc) PUT(", ");
			}
			PUT(")");
			NEXT;
		}

		CODE(PUSH)
        {
			RK_B();
			NEXT;
		}

		CODE(MOV)
        {
			R_A(), PUT(" = "), R_B();
			NEXT;
		}

		CODE(PUT)
        {
			R_A();
			GET_B() > 1 ? PUT(".."), R(GET_A() + GET_B() - 1) : 0;
			NEXT;
		}

		CODE(NIL)
        {
			R_A(), PUT(" = nil");
			NEXT;
		}

		CODE(BOOL)
        {
			R_A(), PUTF(" = %s", GET_sB() ? "true" : "false");
			NEXT;
		}

		CODE(NOT)
        {
			R_A(), PUT(" = !"), RK_B();
			NEXT;
		}

		CODE(NEG)
        {
			R_A(), PUT(" = -"), RK_B();
			NEXT;
		}

		CODE(LT)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" < "), RK_C();
			NEXT;
		}

		CODE(LE)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" <= "), RK_C();
			NEXT;
		}

		CODE(EQ)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" == "), RK_C();
			NEXT;
		}

		CODE(ADD)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" + "), RK_C();
			NEXT;
		}

		CODE(SUB)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" - "), RK_C();
			NEXT;
		}

		CODE(MUL)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" * "), RK_C();
			NEXT;
		}

		CODE(DIV)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" / "), RK_C();
			NEXT;
		}

		CODE(MOD)
        {
			R_A(), PUT(" = "), RK_B(), PUT(" %% "), RK_C();
			NEXT;
		}

		CODE(GLD)
        {
			R_A(), PUT(" = G."), K_B();
			NEXT;
		}

		CODE(GST)
        {
            PUT("G."), K_A(), PUT(" = ");
            GET_sC() ? PUT("nil") : RK_B();
            NEXT;
		}

		CODE(LD)
        {
			R_A(), PUT(" = "), RK_B();
			NEXT;
		}

		CODE(ST)
        {
			R_A(), PUT(" = "), R_B();
			NEXT;
		}

		CODE(CLU)
        {
			R_A();
			NEXT;
		}

		CODE(CLO)
        {
			K_A(); PUTF(", %d", GET_B());
			NEXT;
		}

		CODE(ULD)
        {
			R_A(), PUTF(" = U[%d]", GET_B());
			NEXT;
		}

		CODE(UST)
        {
			PUTF("U[%d] = ", GET_A()), R_B();
			NEXT;
		}

		CODE(JMP)
        {
			PUTF("-> %03d", offset + GET_Ax() + 1);
			NEXT;
		}

		CODE(JMPF)
        {
			PUTF("-> %03d, if !", offset + GET_Ax() + 1), RK_C();
			NEXT;
		}

        CODE_ERR() {
			PUTF("Bad opcode, got %d", GET_Op());
			NEXT;
		}
	}

	printf("\n");
}

void aupCh_dasm(aupCh *chunk, const char *name)
{
	printf("=== %s ===\n", name);

	for (int i = 0; i < chunk->constants.count; i++) {
		printf("K[%d] = ", i);
        aupV_print(chunk->constants.values[i]);
		printf("\n");
	}

	for (int offset = 0; offset < chunk->count; offset++) {
		aupCh_dasmInst(chunk, offset);
	}

	printf("================================================\n\n");
    fflush(stdout);
}