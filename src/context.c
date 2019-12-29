#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "vm.h"

aupCtx *aup_create()
{
    aupCtx *ctx = malloc(sizeof(aupCtx));

    if (ctx != NULL) {
        aupT_init(&ctx->globals);
        aupT_init(&ctx->strings);
    }

    return ctx;
}

void aup_close(aupCtx *ctx)
{
    if (ctx == NULL) return;

    aupT_free(&ctx->globals);
    aupT_free(&ctx->strings);

    free(ctx);
}

aupStt aup_doString(aupCtx *ctx, const char *source)
{
    if (ctx == NULL) return AUP_INVALID;
    if (source == NULL) return AUP_COMPILE_ERROR;

    aupVM *vm = aupVM_new(ctx);
    if (vm != NULL) return AUP_INVALID;

    int ret = aupVM_interpret(vm, source);
    aupVM_free(vm);
    return ret;
}

aupStt aup_doFile(aupCtx *ctx, const char *name)
{
    if (ctx == NULL) return AUP_INVALID;

    char *source = aup_readFile(name, NULL);

    if (source != NULL) {
        int ret = aup_doString(ctx, source);
        free(source);
        return ret;
    }

    return AUP_COMPILE_ERROR;
}
