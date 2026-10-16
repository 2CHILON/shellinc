#ifndef SHELLINC_BUILTINS_H
#define SHELLINC_BUILTINS_H

#include <stdio.h>

#include "parser.h"

typedef struct {
    int status;     /* exit status of the builtin */
    int exit_shell; /* nonzero if the shell should exit */
} BuiltinResult;

/* Returns nonzero if name is a builtin command. */
int builtin_is(const char *name);

/* Runs a builtin. Normal output goes to out; errors go to stderr.
 * last_status is the previous command's status (used by plain "exit"). */
BuiltinResult builtin_run(const Command *c, FILE *out, int last_status);

#endif