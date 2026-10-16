#ifndef SHELLINC_LEXER_H
#define SHELLINC_LEXER_H

#include <stddef.h>

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND
} TokenKind;

typedef struct {
    TokenKind type;
    char *text; /* only set for TOK_WORD, otherwise NULL */
} Token;

typedef struct {
    Token *items;
    size_t count;
} TokenList;

typedef enum {
    LEX_OK,
    LEX_ERR_UNTERMINATED_QUOTE,
    LEX_ERR_NOMEM
} LexStatus;

/* Values needed to expand $? and ~. */
typedef struct {
    int last_status;  /* value of $? */
    const char *home; /* value of ~, or NULL to leave ~ unexpanded */
} ExpandContext;

/* Splits a line into tokens without any expansion.
 * On failure, out is left empty. */
LexStatus lex(const char *line, TokenList *out);

/* Same as lex(), but also expands $NAME, ${NAME}, $? and a leading ~
 * according to the quoting rules. A NULL ctx disables expansion. */
LexStatus lex_expand(const char *line, const ExpandContext *ctx, TokenList *out);

void token_list_free(TokenList *list);

#endif