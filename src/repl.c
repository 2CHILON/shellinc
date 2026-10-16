#include "repl.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "lexer.h"
#include "parser.h"
#include "platform.h"

#define PROMPT "shellinc> "
#define INITIAL_CAP 128

/* Reads one line of any length from stdin.
 * Returns a malloc'd string without the trailing newline (caller frees),
 * or NULL on end-of-input, an interrupted read (Ctrl+C), or allocation failure. */
static char *read_line(void) {
    size_t cap = INITIAL_CAP;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }

    while (fgets(buf + len, (int)(cap - len), stdin)) {
        len += strlen(buf + len);

        if (len > 0 && buf[len - 1] == '\n') {
            buf[--len] = '\0';
            if (len > 0 && buf[len - 1] == '\r') {
                buf[--len] = '\0';
            }
            return buf;
        }

        if (len == cap - 1) { /* buffer full, line not finished: grow */
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                return NULL;
            }
            buf = tmp;
        }
    }

    if (ferror(stdin)) { /* interrupted or failed: never run half a line */
        free(buf);
        return NULL;
    }
    if (len > 0) { /* last line had no trailing newline */
        return buf;
    }
    free(buf);
    return NULL;
}

/* Trims leading and trailing spaces/tabs in place; returns start of result. */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    return s;
}

/* Runs a builtin inside the shell process, honoring > and >> redirection. */
static int run_builtin(const Command *c, int last_status, int *quit) {
    FILE *out = stdout;
    BuiltinResult r;

    if (c->out_file) {
        out = fopen(c->out_file, c->append ? "a" : "w");
        if (!out) {
            fprintf(stderr, "shellinc: %s: %s\n", c->out_file, strerror(errno));
            return 1;
        }
    }

    r = builtin_run(c, out, last_status);

    if (out != stdout) {
        fclose(out);
    }
    if (r.exit_shell) {
        *quit = 1;
    }
    return r.status;
}

int repl_run(void) {
    int last_status = 0;
    int quit = 0;

    platform_init_signals();

    while (!quit) {
        printf(PROMPT);
        fflush(stdout);

        platform_take_interrupt(0); /* forget a Ctrl+C meant for the previous command */
        char *line = read_line();
        if (!line) {
            if (platform_take_interrupt(1)) { /* Ctrl+C at the prompt */
                clearerr(stdin);
                putchar('\n');
                last_status = 130;
                continue;
            }
            putchar('\n'); /* Ctrl+D / Ctrl+Z / closed stdin */
            break;
        }

        char *cmd = trim(line);

        if (*cmd == '\0') {
            free(line);
            continue;
        }

        TokenList tokens;
        ExpandContext ctx = {last_status, platform_home_dir()};
        LexStatus ls = lex_expand(cmd, &ctx, &tokens);
        if (ls != LEX_OK) {
            fprintf(stderr, "shellinc: %s\n",
                    ls == LEX_ERR_UNTERMINATED_QUOTE ? "unterminated quote"
                                                     : "out of memory");
            last_status = 2;
            free(line);
            continue;
        }

        Pipeline pipeline;
        const char *err = NULL;
        if (parse(&tokens, &pipeline, &err) != PARSE_OK) {
            fprintf(stderr, "shellinc: syntax error: %s\n", err);
            last_status = 2;
        } else {
            if (pipeline.count == 1 && builtin_is(pipeline.cmds[0].argv[0])) {
                last_status = run_builtin(&pipeline.cmds[0], last_status, &quit);
            } else {
                last_status = platform_run_pipeline(&pipeline);
            }
            pipeline_free(&pipeline);
        }

        token_list_free(&tokens);
        free(line);
    }
    return last_status;
}