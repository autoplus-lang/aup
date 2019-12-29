#include <stdlib.h>
#include <string.h>

#include "context.h"

aupCtx *aup_create()
{
    aupCtx *ctx = malloc(sizeof(aupCtx));

    aupT_init(&ctx->globals);
    aupT_init(&ctx->strings);
}

void aup_close(aupCtx *ctx)
{
    aupT_free(&ctx->globals);
    aupT_free(&ctx->strings);

    free(ctx);
}

aupStt aup_doString(aupCtx *ctx, const char *source)
{

}

aupStt aup_doFile(aupCtx *ctx, const char *name)
{

}
