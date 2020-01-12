#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "code.h"
#include "value.h"
#include "object.h"

void aup_initChunk(aupChunk *chunk, aupSrc *source)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->columns = NULL;
    chunk->source = source;

    aup_initArray(&chunk->constants);
}

void aup_freeChunk(aupChunk *chunk)
{
    free(chunk->code);
    free(chunk->lines);
    free(chunk->columns);

    aup_freeArray(&chunk->constants);
    aup_initChunk(chunk, NULL);
}

void aup_emitChunk(aupChunk *chunk, uint8_t byte, int line, int column)
{
    if (chunk->count >= chunk->capacity) {
        chunk->capacity += AUP_CODEPAGE;
        chunk->code = realloc(chunk->code, chunk->capacity * sizeof(uint8_t));
        chunk->lines = realloc(chunk->lines, chunk->capacity * sizeof(uint16_t));
        chunk->columns = realloc(chunk->columns, chunk->capacity * sizeof(uint16_t));
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->columns[chunk->count] = column;
    chunk->count++;
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

void aup_dasmChunk(aupChunk *chunk, const char *name) 
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = aup_dasmInstruction(chunk, offset);
    }

    printf("\n");
}

static int constantInst(aupChunk *chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%4d '", constant);
    aup_printValue(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int simpleInst(int offset)
{
    printf("\n");

    return offset + 1;
}

static int byteInst(aupChunk *chunk, int offset)
{
    uint8_t byte = chunk->code[offset + 1];
    printf("%4d\n", byte);

    return offset + 2;
}

static int wordInst(aupChunk *chunk, int offset)
{
    uint16_t word = chunk->code[offset + 1] << 8;
    word |= chunk->code[offset + 2];
    printf("%4d\n", word);

    return offset + 3;
}

static int jumpInst(int sign, aupChunk *chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%4d -> %d\n", offset, offset + 3 + sign * jump);

    return offset + 3;
}

int aup_dasmInstruction(aupChunk *chunk, int offset)
{
    printf("%04d ", offset);

    int line = chunk->lines[offset];
    int column = chunk->columns[offset];

    if (offset > 0 && (line == chunk->lines[offset - 1])) {
        printf("   | ");
    } else {
        printf("%4d:", line);
    }

    if (offset > 0 && (column == chunk->columns[offset - 1])
        && (line == chunk->lines[offset - 1])) {
        printf("|    ");
    }
    else {
        printf("%-4d ", column);
    }

    uint8_t i = chunk->code[offset];
    printf(" %-8s  ", aup_op2Str(i));

    switch (i) {

        case AUP_OP_PRINT:
            return byteInst(chunk, offset);

        case AUP_OP_POP:
            return simpleInst(offset);

        case AUP_OP_NIL:
        case AUP_OP_TRUE:
        case AUP_OP_FALSE:
            return simpleInst(offset);

        case AUP_OP_INT:
            return byteInst(chunk, offset);

        case AUP_OP_INTL:
            return wordInst(chunk, offset);

        case AUP_OP_CONST:
            return constantInst(chunk, offset);

        case AUP_OP_LD:
        case AUP_OP_ST:
            return byteInst(chunk, offset);

        case AUP_OP_DEF:        
        case AUP_OP_GLD:
        case AUP_OP_GST:
            return constantInst(chunk, offset);

        case AUP_OP_ULD:
        case AUP_OP_UST:
            return byteInst(chunk, offset);

        case AUP_OP_GET:
        case AUP_OP_SET:
            return constantInst(chunk, offset);

        case AUP_OP_GETI:
        case AUP_OP_SETI:
            return simpleInst(offset);

        case AUP_OP_MAP:
            return byteInst(chunk, offset);

        case AUP_OP_NOT:
        case AUP_OP_NEG:
            return simpleInst(offset);

        case AUP_OP_LT:
        case AUP_OP_LE:
        case AUP_OP_EQ:
        case AUP_OP_ADD:
        case AUP_OP_SUB:
        case AUP_OP_MUL:
        case AUP_OP_DIV:
        case AUP_OP_MOD:
            return simpleInst(offset);

        case AUP_OP_JMP:
        case AUP_OP_JMPF:
            return jumpInst(1, chunk, offset);

        case AUP_OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%4d '", constant);
            aup_printValue(chunk->constants.values[constant]);
            printf("'\n");

            aupFun *function = AUP_AS_FUN(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d    |                    %s %d\n",
                       offset - 2, isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }

        case AUP_OP_CLOSE:
            return simpleInst(offset);

        case AUP_OP_CALL:
            return byteInst(chunk, offset);

        case AUP_OP_RET:
            return simpleInst(offset);

        default:
            printf("Unknown opcode, got %d.\n", i);
            return offset + 1;
    }
}
