#include <stdio.h>

#include "aup.h"
#include "value.h"


int main(int argc, char *argv[])
{
    aupV v = AUP_BOOL(true);
    v = AUP_NUM(3.5);

    printf("%s\n", AUP_AS_BOOL(v) ? "true" : "false");

    getc(stdin);
}