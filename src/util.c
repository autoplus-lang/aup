#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

char *aup_readFile(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
        return NULL;
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char *buffer = malloc(fileSize + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        return NULL;
	}

	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
        free(buffer);
        fclose(file);
        return NULL;
	}

	buffer[bytesRead] = '\0';
	fclose(file);

    if (size != NULL) *size = bytesRead;
	return buffer;
}
