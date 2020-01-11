#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

uint32_t aup_hashBytes(const void *bytes, int size)
{
    static const int32_t prime = 16777619;
    static const uint32_t basis = 2166136261;

    uint32_t hash = basis;
    const uint8_t *bs = bytes;

    if (size < 0) while (*bs != '\0') {
        hash ^= *(bs++);
        hash *= prime;
    }
    else for (int i = 0; i < size; i++) {
        hash ^= bs[i];
        hash *= prime;
    }

    return hash;
}

char *aup_readFile(const char *path, size_t *size)
{
    FILE *file = NULL;
    char *buffer = NULL;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        goto _clean;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    buffer = malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        goto _clean;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        goto _clean;
    }

    buffer[bytesRead] = '\0';
    fclose(file);

    if (size) *size = bytesRead;
    return buffer;

_clean:
    if (file != NULL) fclose(file);
    if (buffer != NULL) free(buffer);
    return NULL;
}