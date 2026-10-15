#include "lexer.h"

#include <stdlib.h>
#include <string.h>

static int is_space(char c) { return c == ' ' || c == '\t'; }
static int is_operator(char c) { return c == '|' || c == '<' || c == '>'; }

static int is_escapable(char c) {
    return is_space(c) || is_operator(c) || c == '\'' || c == '"' || c == '\\';
}

static int push(TokenList *list, size_t *cap, TokenKind type, char *text) {
    if (list->count == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        Token *tmp = realloc(list->items, new_cap * sizeof *tmp);
        if (!tmp) {
            return 0;
        }
        list->items = tmp;
        *cap = new_cap;
    }
    list->items[list->count].type = type;
    list->items[list->count].text = text;
    list->count++;
    return 1;
}

/* Reads one word starting at *pp, handling quotes and escapes.
 * Returns a malloc'd string, or NULL with *status set. */
static char *read_word(const char **pp, LexStatus *status) {
    const char *p = *pp;
    char *buf = malloc(strlen(p) + 1); /* output is never longer than input */
    size_t n = 0;

    if (!buf) {
        *status = LEX_ERR_NOMEM;
        return NULL;
    }

    while (*p && !is_space(*p) && !is_operator(*p)) {
        if (*p == '\'') {
            p++;
            while (*p && *p != '\'') {
                buf[n++] = *p++;
            }
            if (!*p) {
                free(buf);
                *status = LEX_ERR_UNTERMINATED_QUOTE;
                return NULL;
            }
            p++; /* closing quote */
        } else if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) {
                    p++;
                }
                buf[n++] = *p++;
            }
            if (!*p) {
                free(buf);
                *status = LEX_ERR_UNTERMINATED_QUOTE;
                return NULL;
            }
            p++; /* closing quote */
        } else if (*p == '\\' && is_escapable(p[1])) {
            p++;
            buf[n++] = *p++;
        } else {
            buf[n++] = *p++;
        }
    }

    buf[n] = '\0';
    *pp = p;
    return buf;
}

LexStatus lex(const char *line, TokenList *out) {
    size_t cap = 0;
    const char *p = line;
    LexStatus status = LEX_OK;

    out->items = NULL;
    out->count = 0;

    while (*p) {
        TokenKind type;
        char *text = NULL;

        if (is_space(*p)) {
            p++;
            continue;
        }

        if (*p == '|') {
            type = TOK_PIPE;
            p++;
        } else if (*p == '<') {
            type = TOK_REDIR_IN;
            p++;
        } else if (*p == '>') {
            if (p[1] == '>') {
                type = TOK_REDIR_APPEND;
                p += 2;
            } else {
                type = TOK_REDIR_OUT;
                p++;
            }
        } else {
            type = TOK_WORD;
            text = read_word(&p, &status);
            if (!text) {
                token_list_free(out);
                return status;
            }
        }

        if (!push(out, &cap, type, text)) {
            free(text);
            token_list_free(out);
            return LEX_ERR_NOMEM;
        }
    }
    return LEX_OK;
}

void token_list_free(TokenList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].text);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}