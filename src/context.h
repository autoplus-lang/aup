#ifndef _AUP_CONTEXT_H
#define _AUP_CONTEXT_H
#pragma once

#include "util.h"
#include "table.h"
#include "object.h"

struct _aupCtx {
    aupT strings;
    aupT globals;
};

aupCtx *aup_create();
void aup_close(aupCtx *ctx);

aupStt aup_doString(aupCtx *ctx, const char *source);
aupStt aup_doFile(aupCtx *ctx, const char *name);

#endif
