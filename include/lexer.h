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

/* Splits a line into tokens. On failure, out is left empty. */
LexStatus lex(const char *line, TokenList *out);

void token_list_free(TokenList *list);

#endif