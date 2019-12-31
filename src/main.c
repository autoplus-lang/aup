#include <stdio.h>
#include "vm.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("usage: aup [file]");
		return 0;
	}
	
    int ret = 0;
    const char *file = argv[argc - 1];

    aupVM *vm = aup_create(NULL);

    if (vm != NULL) {
        ret = aup_doFile(vm, file);
        aup_close(vm);
    }
    else {
        ret = 1;
        fprintf(stderr, "Could not create new virtual machine instance!\n");
    }
	
	return ret;
}
