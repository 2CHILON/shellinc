#include <stdio.h>

#include "repl.h"

int main(void) {
    printf("shellinc %s\n", SHELLINC_VERSION);
    return repl_run();
}