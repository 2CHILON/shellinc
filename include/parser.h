#ifndef SHELLINC_PARSER_H
#define SHELLINC_PARSER_H

#include <stddef.h>

#include "lexer.h"

typedef struct {
    char **argv;    /* NULL-terminated */
    size_t argc;
    char *in_file;  /* NULL if no < redirection */
    char *out_file; /* NULL if no > or >> redirection */
    int append;     /* 1 for >>, 0 for > */
} Command;

typedef struct {
    Command *cmds;
    size_t count;
} Pipeline;

typedef enum {
    PARSE_OK,
    PARSE_ERR_SYNTAX,
    PARSE_ERR_NOMEM
} ParseStatus;

/* Builds a pipeline from tokens. An empty token list gives an empty pipeline
 * (count == 0) and PARSE_OK. On error, out is left empty and *err (if not
 * NULL) points to a static message. The pipeline owns copies of all strings. */
ParseStatus parse(const TokenList *tokens, Pipeline *out, const char **err);

void pipeline_free(Pipeline *p);

#endif