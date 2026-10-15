#include "parser.h"

#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy) {
        memcpy(copy, s, n);
    }
    return copy;
}

static void command_init(Command *c) {
    memset(c, 0, sizeof *c);
}

static void command_free(Command *c) {
    for (size_t i = 0; i < c->argc; i++) {
        free(c->argv[i]);
    }
    free(c->argv);
    free(c->in_file);
    free(c->out_file);
}

static int command_add_arg(Command *c, const char *s) {
    char *copy = dup_str(s);
    char **tmp;

    if (!copy) {
        return 0;
    }
    tmp = realloc(c->argv, (c->argc + 2) * sizeof *tmp);
    if (!tmp) {
        free(copy);
        return 0;
    }
    c->argv = tmp;
    c->argv[c->argc++] = copy;
    c->argv[c->argc] = NULL;
    return 1;
}

static int pipeline_push(Pipeline *p, const Command *c) {
    Command *tmp = realloc(p->cmds, (p->count + 1) * sizeof *tmp);
    if (!tmp) {
        return 0;
    }
    p->cmds = tmp;
    p->cmds[p->count++] = *c; /* ownership moves to the pipeline */
    return 1;
}

void pipeline_free(Pipeline *p) {
    for (size_t i = 0; i < p->count; i++) {
        command_free(&p->cmds[i]);
    }
    free(p->cmds);
    p->cmds = NULL;
    p->count = 0;
}

ParseStatus parse(const TokenList *tokens, Pipeline *out, const char **err) {
    ParseStatus status = PARSE_ERR_SYNTAX;
    const char *msg = "syntax error";
    Command cur;

    out->cmds = NULL;
    out->count = 0;
    command_init(&cur);

    if (tokens->count == 0) {
        return PARSE_OK;
    }

    for (size_t i = 0; i < tokens->count; i++) {
        const Token *t = &tokens->items[i];

        switch (t->type) {
        case TOK_WORD:
            if (!command_add_arg(&cur, t->text)) {
                goto nomem;
            }
            break;

        case TOK_REDIR_IN:
        case TOK_REDIR_OUT:
        case TOK_REDIR_APPEND: {
            char *copy;
            if (i + 1 >= tokens->count || tokens->items[i + 1].type != TOK_WORD) {
                msg = "expected a file name after redirection";
                goto syntax;
            }
            copy = dup_str(tokens->items[++i].text);
            if (!copy) {
                goto nomem;
            }
            if (t->type == TOK_REDIR_IN) {
                free(cur.in_file);
                cur.in_file = copy;
            } else {
                free(cur.out_file);
                cur.out_file = copy;
                cur.append = (t->type == TOK_REDIR_APPEND);
            }
            break;
        }

        case TOK_PIPE:
            if (cur.argc == 0) {
                msg = "missing command before '|'";
                goto syntax;
            }
            if (!pipeline_push(out, &cur)) {
                goto nomem;
            }
            command_init(&cur);
            break;
        }
    }

    if (cur.argc == 0) {
        msg = out->count > 0 ? "missing command after '|'" : "missing command";
        goto syntax;
    }
    if (!pipeline_push(out, &cur)) {
        goto nomem;
    }
    return PARSE_OK;

nomem:
    status = PARSE_ERR_NOMEM;
    msg = "out of memory";
syntax:
    command_free(&cur);
    pipeline_free(out);
    if (err) {
        *err = msg;
    }
    return status;
}