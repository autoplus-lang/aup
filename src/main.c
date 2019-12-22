#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "aup.h"
#include "vm.h"

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

	clock_t clk = clock();
	
	char *source = readFile(argv[argc - 1]);
	aupVM *vm = aupVM_new();

	int ret = aup_interpret(vm, source);

	if (ret != AUP_OK)
		printf("\n%s error!\n", ret == AUP_COMPILE_ERR ? "Compile" : "Runtime");

	aupVM_free(vm);
	free(source);

	printf("\ndone in %dms!\n", clock() - clk);
	return 0;
}