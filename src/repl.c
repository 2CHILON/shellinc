#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROMPT "shellinc> "
#define INITIAL_CAP 128

/* Reads one line of any length from stdin.
 * Returns a malloc'd string without the trailing newline (caller frees),
 * or NULL on end-of-input or allocation failure. */
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

int repl_run(void) {
    for (;;) {
        printf(PROMPT);
        fflush(stdout);

        char *line = read_line();
        if (!line) { /* Ctrl+D / Ctrl+Z / closed stdin */
            putchar('\n');
            break;
        }

        char *cmd = trim(line);

        if (*cmd == '\0') {
            free(line);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            free(line);
            break;
        }

        printf("shellinc: not implemented yet: %s\n", cmd);
        free(line);
    }
    return 0;
}