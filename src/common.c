#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

uint32_t aup_hashBytes(const void *bytes, size_t size)
{
    uint32_t hash = 2166136261u;
    const uint8_t *bs = (const uint8_t *)bytes;

    for (size_t i = 0; i < size; i++) {
        hash ^= bs[i];
        hash *= 16777619;
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