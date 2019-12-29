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
    if (ctx == NULL) return;
}

aupStt aup_doFile(aupCtx *ctx, const char *name)
{

}
