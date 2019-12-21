#include <stdio.h>
#include <stdlib.h>

#include "aup.h"
#include "compiler.h"

static char *readFile(const char* path)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char *buffer = (char*)malloc(fileSize + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}

	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytesRead] = '\0';
	fclose(file);
	return buffer;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("usage: aup [file]");
		return 0;
	}

	aupCh chunk;
	char *source;
	
	source = readFile(argv[argc - 1]);
	aupCh_init(&chunk);

	if (!aup_compile(source, &chunk)) {
		puts("\nCompile error!");
	}

	aupCh_free(&chunk);
	free(source);
	return 0;
}