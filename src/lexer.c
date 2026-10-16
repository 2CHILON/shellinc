#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A small growable string used to build one word. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static int sb_init(StrBuf *sb) {
    sb->cap = 32;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (!sb->data) {
        return 0;
    }
    sb->data[0] = '\0';
    return 1;
}

static int sb_append(StrBuf *sb, const char *s, size_t n) {
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap;
        char *tmp;

        while (sb->len + n + 1 > cap) {
            cap *= 2;
        }
        tmp = realloc(sb->data, cap);
        if (!tmp) {
            return 0;
        }
        sb->data = tmp;
        sb->cap = cap;
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 1;
}

static int sb_putc(StrBuf *sb, char c) {
    return sb_append(sb, &c, 1);
}

static int is_space(char c) { return c == ' ' || c == '\t'; }
static int is_operator(char c) { return c == '|' || c == '<' || c == '>'; }
static int is_name_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_name_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

/* Characters a backslash can escape outside quotes. $ is only special when
 * expansion is on, so Windows paths keep their backslashes either way. */
static int is_escapable(char c, const ExpandContext *x) {
    return is_space(c) || is_operator(c) || c == '\'' || c == '"' || c == '\\' ||
           (x && c == '$');
}

/* True if a ~ followed by c should expand to the home directory. */
static int tilde_end(char c) {
    if (c == '\0' || c == '/' || is_space(c) || is_operator(c)) {
        return 1;
    }
#ifdef _WIN32
    if (c == '\\') {
        return 1;
    }
#endif
    return 0;
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

/* Handles a $ at *pp: appends the expansion (or a literal $) to sb and moves
 * *pp past what was consumed. Sets *expanded when a real expansion happened.
 * Returns 0 on out-of-memory. */
static int expand_dollar(const char **pp, const ExpandContext *x, StrBuf *sb, int *expanded) {
    const char *p = *pp + 1; /* the character after the $ */
    const char *start;
    const char *end;
    const char *after;
    const char *value = NULL;
    char name[256];
    size_t n;

    if (*p == '?') {
        char num[16];
        snprintf(num, sizeof num, "%d", x->last_status);
        *pp = p + 1;
        *expanded = 1;
        return sb_append(sb, num, strlen(num));
    }

    if (*p == '{' && is_name_start(p[1])) {
        start = p + 1;
        end = start;
        while (is_name_char(*end)) {
            end++;
        }
        if (*end != '}') { /* unterminated ${...: keep the $ literally */
            *pp = *pp + 1;
            return sb_putc(sb, '$');
        }
        after = end + 1;
    } else if (is_name_start(*p)) {
        start = p;
        end = start;
        while (is_name_char(*end)) {
            end++;
        }
        after = end;
    } else { /* a lone $ or $ followed by something that is not a name */
        *pp = *pp + 1;
        return sb_putc(sb, '$');
    }

    n = (size_t)(end - start);
    if (n < sizeof name) {
        memcpy(name, start, n);
        name[n] = '\0';
        value = getenv(name);
    }
    *pp = after;
    *expanded = 1;
    return value ? sb_append(sb, value, strlen(value)) : 1;
}

/* Reads one word starting at *pp, handling quotes, escapes and (when x is not
 * NULL) expansion. On success *out is a malloc'd string, or NULL when an
 * unquoted word expanded to nothing and must be dropped. */
static LexStatus read_word(const char **pp, const ExpandContext *x, char **out) {
    const char *p = *pp;
    StrBuf sb;
    int quoted = 0;
    int expanded = 0;

    *out = NULL;
    if (!sb_init(&sb)) {
        return LEX_ERR_NOMEM;
    }

    if (x && x->home && p[0] == '~' && tilde_end(p[1])) {
        if (!sb_append(&sb, x->home, strlen(x->home))) {
            goto nomem;
        }
        p++;
    }

    while (*p && !is_space(*p) && !is_operator(*p)) {
        if (*p == '\'') { /* single quotes: everything literal */
            quoted = 1;
            p++;
            while (*p && *p != '\'') {
                if (!sb_putc(&sb, *p++)) {
                    goto nomem;
                }
            }
            if (!*p) {
                free(sb.data);
                return LEX_ERR_UNTERMINATED_QUOTE;
            }
            p++; /* closing quote */
        } else if (*p == '"') { /* double quotes: $ expands, \" \\ \$ escape */
            quoted = 1;
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || (x && p[1] == '$'))) {
                    p++;
                    if (!sb_putc(&sb, *p++)) {
                        goto nomem;
                    }
                } else if (x && *p == '$') {
                    if (!expand_dollar(&p, x, &sb, &expanded)) {
                        goto nomem;
                    }
                } else if (!sb_putc(&sb, *p++)) {
                    goto nomem;
                }
            }
            if (!*p) {
                free(sb.data);
                return LEX_ERR_UNTERMINATED_QUOTE;
            }
            p++; /* closing quote */
        } else if (*p == '\\' && is_escapable(p[1], x)) {
            p++;
            if (!sb_putc(&sb, *p++)) {
                goto nomem;
            }
        } else if (x && *p == '$') {
            if (!expand_dollar(&p, x, &sb, &expanded)) {
                goto nomem;
            }
        } else if (!sb_putc(&sb, *p++)) {
            goto nomem;
        }
    }

    *pp = p;
    if (sb.len == 0 && !quoted && expanded) { /* e.g. an unset $VAR on its own */
        free(sb.data);
        return LEX_OK;
    }
    *out = sb.data;
    return LEX_OK;

nomem:
    free(sb.data);
    return LEX_ERR_NOMEM;
}

static LexStatus lex_impl(const char *line, const ExpandContext *x, TokenList *out) {
    size_t cap = 0;
    const char *p = line;

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
            LexStatus st;

            type = TOK_WORD;
            st = read_word(&p, x, &text);
            if (st != LEX_OK) {
                token_list_free(out);
                return st;
            }
            if (!text) { /* the word expanded to nothing: drop it */
                continue;
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

LexStatus lex(const char *line, TokenList *out) {
    return lex_impl(line, NULL, out);
}

LexStatus lex_expand(const char *line, const ExpandContext *ctx, TokenList *out) {
    return lex_impl(line, ctx, out);
}

void token_list_free(TokenList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].text);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}