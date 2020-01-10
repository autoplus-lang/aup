#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "code.h"
#include "value.h"

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

    source->fname = strdup(s);
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