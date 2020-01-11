#include <math.h>
#include <time.h>
#include <stdlib.h>

#include "vm.h"
#include "value.h"
#include "object.h"

static aupVal math_abs(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double n = AUP_AS_NUM(args[0]);
    return AUP_NUM(n < 0 ? (-n) : n);
}

static aupVal math_ceil(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(ceil(x));
}

static aupVal math_cos(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(cos(x));
}

static aupVal math_floor(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(floor(x));
}

static aupVal math_log(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(log(x));
}

static aupVal math_log10(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(log10(x));
}

static aupVal math_pow(aupVM *vm, int argc, aupVal *args)
{
    if (!AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");
    if (argc < 2 || !AUP_IS_NUM(args[1]))
        return aup_error(vm, "#2 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    double y = AUP_AS_NUM(args[1]);
    return AUP_NUM(pow(x, y));
}

static aupVal math_rand(aupVM *vm, int argc, aupVal *args)
{
    static bool seeded = false;
    if (!seeded) srand((unsigned int)time(NULL)), seeded = true;

    if (argc >= 1) {
        double min, max;

        if (!AUP_IS_NUM(args[0]))
            return aup_error(vm, "#1 must be a number.");

        min = 0;
        max = AUP_AS_NUM(args[0]);

        if (argc == 2) {
            if (!AUP_IS_NUM(args[1]))
                return aup_error(vm, "#2 must be a number.");
            min = AUP_AS_NUM(args[0]);
            max = AUP_AS_NUM(args[1]);
        }

        return AUP_NUM( ((double)rand() * (max - min)) / (double)RAND_MAX + min );
    }

    return AUP_NUM((double)rand() / (double)RAND_MAX);
}

static aupVal math_sin(aupVM *vm, int argc, aupVal *args)
{
    if (!AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(sin(x));
}

static aupVal math_sqrt(aupVM *vm, int argc, aupVal *args)
{
    if (!AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(sqrt(x));
}

void aup_loadMath(aupVM *vm)
{
    aupMap *math = aup_newMap(vm);

    aup_setMap(vm, math, "nan", AUP_NUM(NAN));
    aup_setMap(vm, math, "inf", AUP_NUM(INFINITY));

    aup_setMap(vm, math, "abs",     AUP_CFN(math_abs));
    aup_setMap(vm, math, "ceil",    AUP_CFN(math_ceil));
    aup_setMap(vm, math, "cos",     AUP_CFN(math_cos));
    aup_setMap(vm, math, "floor",   AUP_CFN(math_floor));
    aup_setMap(vm, math, "log",     AUP_CFN(math_log));
    aup_setMap(vm, math, "log10",   AUP_CFN(math_log10));
    aup_setMap(vm, math, "pow",     AUP_CFN(math_pow));
    aup_setMap(vm, math, "rand",    AUP_CFN(math_rand));
    aup_setMap(vm, math, "sin",     AUP_CFN(math_sin));
    aup_setMap(vm, math, "sqrt",    AUP_CFN(math_sqrt));

    aup_setGlobal(vm, "math", AUP_OBJ(math));
}
