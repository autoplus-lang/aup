#include <stdio.h>

#include "context.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		puts("usage: aup [file]");
		return 0;
	}
	
    int ret = 0;
    const char *file = argv[argc - 1];

    aupCtx *ctx = aup_create();

    if (ctx != NULL) {
        ret = aup_doFile(ctx, file);
        aup_close(ctx);
    }
    else {
        ret = 1;
        fprintf(stderr, "Could not create new context!\n");
    }
	
	return ret;
}
