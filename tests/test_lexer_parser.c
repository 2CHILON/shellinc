#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

static int failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                  \
        }                                                                \
    } while (0)

#define REQUIRE(cond)                                                    \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                  \
            return;                                                      \
        }                                                                \
    } while (0)

static int parse_ok(const char *line, TokenList *toks, Pipeline *pl) {
    if (lex(line, toks) != LEX_OK) {
        return 0;
    }
    if (parse(toks, pl, NULL) != PARSE_OK) {
        token_list_free(toks);
        return 0;
    }
    return 1;
}

static int is_rejected(const char *line) {
    TokenList t;
    Pipeline p;
    ParseStatus st;

    if (lex(line, &t) != LEX_OK) {
        return 1;
    }
    st = parse(&t, &p, NULL);
    if (st == PARSE_OK) {
        pipeline_free(&p);
    }
    token_list_free(&t);
    return st != PARSE_OK;
}

static void test_simple(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("ls -la", &t, &p));
    REQUIRE(p.count == 1);
    CHECK(p.cmds[0].argc == 2);
    CHECK(strcmp(p.cmds[0].argv[0], "ls") == 0);
    CHECK(strcmp(p.cmds[0].argv[1], "-la") == 0);
    CHECK(p.cmds[0].argv[2] == NULL);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_blank(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("   \t ", &t, &p));
    CHECK(p.count == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_quotes(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("echo \"hello world\" 'a  b'", &t, &p));
    REQUIRE(p.cmds[0].argc == 3);
    CHECK(strcmp(p.cmds[0].argv[1], "hello world") == 0);
    CHECK(strcmp(p.cmds[0].argv[2], "a  b") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_quote_joining_and_empty(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("echo a\"b c\"d \"\"", &t, &p));
    REQUIRE(p.cmds[0].argc == 3);
    CHECK(strcmp(p.cmds[0].argv[1], "ab cd") == 0);
    CHECK(strcmp(p.cmds[0].argv[2], "") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_escapes(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("echo a\\ b", &t, &p));
    REQUIRE(p.cmds[0].argc == 2);
    CHECK(strcmp(p.cmds[0].argv[1], "a b") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_windows_path(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("cd C:\\Users\\ADMIN", &t, &p));
    REQUIRE(p.cmds[0].argc == 2);
    CHECK(strcmp(p.cmds[0].argv[1], "C:\\Users\\ADMIN") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_pipes(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("cat f | grep x | wc -l", &t, &p));
    REQUIRE(p.count == 3);
    CHECK(strcmp(p.cmds[0].argv[0], "cat") == 0);
    CHECK(strcmp(p.cmds[2].argv[1], "-l") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_redirects(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("sort < in.txt > out.txt", &t, &p));
    REQUIRE(p.count == 1);
    CHECK(strcmp(p.cmds[0].in_file, "in.txt") == 0);
    CHECK(strcmp(p.cmds[0].out_file, "out.txt") == 0);
    CHECK(p.cmds[0].append == 0);
    CHECK(p.cmds[0].argc == 1);
    pipeline_free(&p);
    token_list_free(&t);

    REQUIRE(parse_ok("echo hi >> log", &t, &p));
    CHECK(strcmp(p.cmds[0].out_file, "log") == 0);
    CHECK(p.cmds[0].append == 1);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_operators_without_spaces(void) {
    TokenList t;
    Pipeline p;
    REQUIRE(parse_ok("ls|wc>out", &t, &p));
    REQUIRE(p.count == 2);
    CHECK(strcmp(p.cmds[1].argv[0], "wc") == 0);
    CHECK(strcmp(p.cmds[1].out_file, "out") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_errors(void) {
    TokenList t;
    CHECK(lex("echo \"oops", &t) == LEX_ERR_UNTERMINATED_QUOTE);
    CHECK(lex("echo 'oops", &t) == LEX_ERR_UNTERMINATED_QUOTE);
    CHECK(is_rejected("| ls"));
    CHECK(is_rejected("ls |"));
    CHECK(is_rejected("ls | | wc"));
    CHECK(is_rejected("ls >"));
    CHECK(is_rejected("ls > | wc"));
    CHECK(is_rejected("> out"));
}

int main(void) {
    test_simple();
    test_blank();
    test_quotes();
    test_quote_joining_and_empty();
    test_escapes();
    test_windows_path();
    test_pipes();
    test_redirects();
    test_operators_without_spaces();
    test_errors();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all lexer/parser tests passed\n");
    return 0;
}