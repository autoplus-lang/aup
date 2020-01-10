#include <stdio.h>

#include "vm.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: aup [file]\n");
        return 0;
    }

    aupVM *vm = aup_create();
    int ret = AUP_INIT_ERROR;

    if (vm != NULL) {
        ret = aup_doFile(vm, argv[argc - 1]);
        aup_close(vm);
    }

    return ret;
}
