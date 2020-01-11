#include <math.h>
#include <time.h>
#include <stdlib.h>

#include "vm.h"
#include "value.h"
#include "object.h"

/*
    math.abs(num) -> num
*/
static aupVal math_abs(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double n = AUP_AS_NUM(args[0]);
    return AUP_NUM(n < 0 ? (-n) : n);
}

/*
    math.ceil(num) -> num
*/
static aupVal math_ceil(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(ceil(x));
}

/*
    math.cos(num) -> num
*/
static aupVal math_cos(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(cos(x));
}

/*
    math.floor(num) -> num
*/
static aupVal math_floor(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(floor(x));
}

/*
    math.log(num) -> num
*/
static aupVal math_log(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(log(x));
}

/*
    math.log10(num) -> num
*/
static aupVal math_log10(aupVM *vm, int argc, aupVal *args)
{
    if (argc < 1 || !AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(log10(x));
}

/*
    math.pow(num, num) -> num
*/
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

/*
    math.log() -> num
    math.log(num) -> num
    math.log(num, num) -> num
*/
static aupVal math_rand(aupVM *vm, int argc, aupVal *args)
{
    if (argc >= 1) {
        int high, low;

        if (!AUP_IS_NUM(args[0]))
            return aup_error(vm, "#1 must be a number.");

        low = 0;
        high = AUP_AS_INT(args[0]);

        if (argc == 2) {
            if (!AUP_IS_NUM(args[1]))
                return aup_error(vm, "#2 must be a number.");

            low = AUP_AS_INT(args[0]);
            high = AUP_AS_INT(args[1]);
        }

        return AUP_NUM(rand() % (high + 1 - low) + low);
    }
    
    double ret = (double)rand() / (double)RAND_MAX;     
    return AUP_NUM(ceil(ret * 100) / 100);
}

/*
    math.sin(num) -> num
*/
static aupVal math_sin(aupVM *vm, int argc, aupVal *args)
{
    if (!AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(sin(x));
}

/*
    math.sqrt(num) -> num
*/
static aupVal math_sqrt(aupVM *vm, int argc, aupVal *args)
{
    if (!AUP_IS_NUM(args[0]))
        return aup_error(vm, "#1 must be a number.");

    double x = AUP_AS_NUM(args[0]);
    return AUP_NUM(sqrt(x));
}

// math.nan -> num (nan)
static const int math_nan = 0x7F800001;

// math.inf -> num (inf)
static const int math_inf = 0x7F800000;

// math.pi -> num

void aup_loadMath(aupVM *vm)
{
    srand((unsigned)time(NULL));
    rand();

    aupMap *math = aup_newMap(vm);

    aup_setMap(vm, math, "pi",      AUP_NUM(acos(-1)));
    aup_setMap(vm, math, "nan",     AUP_NUM(*(float *)&math_nan));
    aup_setMap(vm, math, "inf",     AUP_NUM(*(float *)&math_inf));

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
