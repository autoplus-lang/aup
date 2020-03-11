#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code.h"

void aup_initChunk(aupChunk *chunk, aupSrc *source)
{
    memset(chunk, '\0', sizeof(aupChunk));
    chunk->source = source;
}

void aup_freeChunk(aupChunk *chunk)
{
    free(chunk->code);
    free(chunk->lines);
    free(chunk->columns);
    aup_freeArray(&chunk->constants);
}

int aup_emitChunk(aupChunk *chunk, uint32_t inst, int line, int column)
{
    int count = chunk->count++;

    if (count >= chunk->space) {
        int space = chunk->space = aup_growCap(chunk->space);
        chunk->code = realloc(chunk->code,
            sizeof(uint32_t) * space);
        chunk->lines = realloc(chunk->lines,
            sizeof(uint16_t) * space);
        chunk->columns = realloc(chunk->columns,
            sizeof(uint16_t) * space);
    }
   
    chunk->code[count] = inst;
    chunk->lines[count] = line;
    chunk->columns[count] = column;

    return count;
}

int aup_addConstant(aupChunk *chunk, aupVal val)
{
    return aup_pushArray(&chunk->constants, val, false);
}

aupSrc *aup_newSource(const char *fname)
{
    aupSrc *source = malloc(sizeof(aupSrc));
    if (source == NULL) return NULL;

    char *buffer = aup_readFile(fname, &source->size);
    if (buffer == NULL) {
        free(source);
        return NULL;
    }

    const char *s;
    if ((s = strrchr(fname, '/')) != NULL) s++;
    if ((s = strrchr(fname, '\\')) != NULL) s++;
    if (s == NULL) s = fname;

    size_t len = strlen(s);
    char *bufname = malloc((len + 1) * sizeof(char));
    memcpy(bufname, s, len + 1);
    bufname[len] = '\0';

    source->fname = bufname;
    source->buffer = buffer;
    return source;
}

void aup_freeSource(aupSrc *source)
{
    if (source != NULL) {
        free(source->fname);
        free(source->buffer);
        free(source);
    }
}

int aup_dasmInst(aupChunk *chunk, int offset)
{
    uint32_t i = chunk->code[offset];

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

#define Op      aup_getOp(i)
#define A       aup_getA(i)
#define B       aup_getB(i)
#define C       aup_getC(i)
#define Bx      aup_getBx(i)
#define Cx      aup_getCx(i)
#define sB      aup_getsB(i)
#define sC      aup_getsC(i)
#define Axx     aup_getAxx(i)

#define R(i)    printf("R[%d]", i)
#define K(i)    printf("K[%d]", i)

#define RA      R(A)
#define RB      R(B)
#define RC      R(C)

#define KA      K(A)
#define KB      K(B)
#define KC      K(C)

#define RKB     (sB ? KB : RB)
#define RKC     (sC ? KC : RC)

#define PUT(x)      printf(x)
#define PUTF(f,...) printf(f, ##__VA_ARGS__)

#define DISPATCH    switch (Op)
#define CODE(x)		case AUP_OP_##x:
#define CODE_ERR()  default:
#define NEXT		break

    printf("%-6s ", aup_opName(Op));
    printf(" %2X %3X %3X ; ", A, Bx, Cx);

    DISPATCH
    {
        CODE(PRI)
        {
            PUTF("(%d) ", B);
            RA;
            B > 1 ? PUT(".."), R(A + B - 1) : 0;
            NEXT;
        }

        CODE(PSH)
        {
            NEXT;
        }
        CODE(POP)
        {
            NEXT;
        }

        CODE(NIL)
        {
            RA, PUT(" = nil");
            NEXT;
        }
        CODE(BOOL)
        {
            RA, PUTF(" = %s", sB ? "true" : "false");
            NEXT;
        }

        CODE(CALL)
        {
            RA, PUT(" = "), RA, PUTF("(%d)", B);
            NEXT;
        }
        CODE(RET)
        {
            A ? RKB : PUT("nil");
            NEXT;
        }

        CODE(JMP)
        {
            PUTF("-> %03d.", offset + Axx + 1);
            NEXT;
        }
        CODE(JMPF)
        {
            PUTF("-> %03d. if !", offset + Axx + 1), RKC;
            NEXT;
        }
        CODE(JNE)
        {
            PUTF("-> %03d. if ", offset + Axx + 1), R(C), PUT(" != "), R(C - 1);
            NEXT;
        }

        CODE(NOT)
        {
            RA, PUT(" = !"), RKB;
            NEXT;
        }
        CODE(LT)
        {
            RA, PUT(" = "), RKB, PUT(" < "), RKC;
            NEXT;
        }
        CODE(LE)
        {
            RA, PUT(" = "), RKB, PUT(" <= "), RKC;
            NEXT;
        }
        CODE(GT)
        {
            RA, PUT(" = "), RKB, PUT(" > "), RKC;
            NEXT;
        }
        CODE(GE)
        {
            RA, PUT(" = "), RKB, PUT(" >= "), RKC;
            NEXT;
        }
        CODE(EQ)
        {
            RA, PUT(" = "), RKB, PUT(" == "), RKC;
            NEXT;
        }

        CODE(NEG)
        {
            RA, PUT(" = -"), RKB;
            NEXT;
        }
        CODE(ADD)
        {
            RA, PUT(" = "), RKB, PUT(" + "), RKC;
            NEXT;
        }
        CODE(SUB) {
            RA, PUT(" = "), RKB, PUT(" - "), RKC;
            NEXT;
        }
        CODE(MUL) {
            RA, PUT(" = "), RKB, PUT(" * "), RKC;
            NEXT;
        }
        CODE(DIV) {
            RA, PUT(" = "), RKB, PUT(" / "), RKC;
            NEXT;
        }
        CODE(MOD)
        {
            RA, PUT(" = "), RKB, PUT(" %% "), RKC;
            NEXT;
        }
        CODE(POW)
        {
            RA, PUT(" = "), RKB, PUT(" ** "), RKC;
            NEXT;
        }

        CODE(BNOT)
        {
            RA, PUT(" = ~"), RKB;
            NEXT;
        }
        CODE(BAND)
        {
            RA, PUT(" = "), RKB, PUT(" & "), RKC;
            NEXT;
        }
        CODE(BOR)
        {
            RA, PUT(" = "), RKB, PUT(" | "), RKC;
            NEXT;
        }
        CODE(BXOR)
        {
            RA, PUT(" = "), RKB, PUT(" ^ "), RKC;
            NEXT;
        }
        CODE(SHL)
        {
            RA, PUT(" = "), RKB, PUT(" << "), RKC;
            NEXT;
        }
        CODE(SHR)
        {
            RA, PUT(" = "), RKB, PUT(" >> "), RKC;
            NEXT;
        }

        CODE(MOV)
        {
            RA, PUT(" = "), RB;
            NEXT;
        }
        CODE(LD)
        {
            RA, PUT(" = "), RKB;
            NEXT;
        }

        CODE(GLD)
        {
            RA, PUT(" = G."), KB;
            NEXT;
        }
        CODE(GST)
        {
            PUT("G."), KA, PUT(" = ");
            sC ? PUT("nil") : RKB;
            NEXT;
        }

        CODE(GET)
        {
        }
        CODE(SET)
        {
        }

        CODE_ERR()
        {
            PUTF("Bad opcode, got %3d.", Op);
            NEXT;
        }
    }

    fflush(stdout);
    return offset + 1;
}

void aup_dasmChunk(aupChunk *chunk, const char *name)
{
    printf("=== %s ===\n", name);

    for (int i = 0; i < chunk->constants.count; i++) {
        printf("K[%d] = ", i);
        aup_printValue(chunk->constants.values[i]);
        printf("\n");
    }

    printf("off  ln col  op     A  Bx  Cx   comments           \n");
    printf("--- --- --- ------------------ --------------------\n");

    for (int offset = 0; offset < chunk->count;) {
        offset = aup_dasmInst(chunk, offset);
        printf("\n");
    }

    printf("\n");
}
