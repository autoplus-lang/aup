#include <stdio.h>
#include "vm.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: aup [file]\n");
        return 0;
    }

    aupSrc *source = aup_newSource(argv[1]);
    if (source != NULL) {
        aupVM *vm = aup_createVM(NULL);
        aup_interpret(vm, source);

        aup_closeVM(vm);
        aup_freeSource(source);
    }

    return 0;
}
