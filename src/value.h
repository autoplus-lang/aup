#include "util.h"

typedef union {
    uint64_t b;
    uint32_t bs[2];
    double f;
} aupN;

typedef uint64_t aupV;

#define AUP_SBIT        ((uint64_t)1 << 63)
#define AUP_QNAN        ((uint64_t)0x7ffc000000000000ULL)

#define AUP_TAG_NIL     1
#define AUP_TAG_FALSE   2
#define AUP_TAG_TRUE    3

#define AUP_NIL         ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_NIL))
#define AUP_TRUE        ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_TRUE))
#define AUP_FALSE       ((aupV)(uint64_t)(AUP_QNAN | AUP_TAG_FALSE))
#define AUP_BOOL(b)     ((b) ? AUP_TRUE : AUP_FALSE)
#define AUP_NUM(n)      ((aupN){.f =(n)}).b
#define AUP_OBJ(o)      (aupV)(AUP_SBIT | AUP_QNAN | (uint64_t)(uintptr_t)(o))

#define AUP_IS_NIL(v)   ((v) == AUP_NIL)
#define AUP_IS_BOOL(v)  (((v) & (AUP_QNAN | AUP_TAG_FALSE)) == (AUP_QNAN | AUP_TAG_FALSE))
#define AUP_IS_NUM(v)   (((v) & AUP_QNAN) != AUP_QNAN)
#define AUP_IS_OBJ(v)   (((v) & (AUP_QNAN | AUP_SBIT)) == (AUP_QNAN | AUP_SBIT))

#define AUP_AS_BOOL(v)  ((v) == AUP_TRUE)
#define AUP_AS_NUM(v)   ((aupN){.b=(v)}).f
#define AUP_AS_OBJ(v)   ((Obj*)(uintptr_t)((v) & ~(AUP_SBIT | AUP_QNAN)))
