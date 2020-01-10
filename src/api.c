#include <stdlib.h>
#include <string.h>

#include "aup.h"
#include "common.h"
#include "vm.h"

void aup_defineNative(aupVM *vm, const char *name, aupCFn function)
{
    if (vm->hadError) return;
    aupStr *global = aup_copyString(vm, name, (int)strlen(name));
    aup_pushRoot(vm, (aupObj *)global);
    aup_setTable(vm->globals, global, AUP_CFN(function));
    aup_popRoot(vm);
}

void aup_setGlobal(aupVM *vm, const char *name, aupVal value)
{
    if (vm->hadError) return;
    aupStr *global = aup_copyString(vm, name, (int)strlen(name));
    aup_push(vm, value);
    aup_pushRoot(vm, (aupObj *)global);
    aup_setTable(vm->globals, global, value);
    aup_popRoot(vm);
    aup_pop(vm);
}

aupVal aup_getGlobal(aupVM *vm, const char *name)
{
    aupVal value = AUP_NIL;
    aupStr *global = aup_copyString(vm, name, (int)strlen(name));
    aup_pushRoot(vm, (aupObj *)global);
    aup_getTable(vm->globals, global, &value);
    aup_popRoot(vm);
    return value;
}
