#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "platform.h"

static int failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static const ExpandContext ctx = {42, "/home/test"};
static const ExpandContext no_home = {0, NULL};

/* Lexes and parses line with expansion and compares the single command's
 * arguments to the expected ones. */
static void expect(const char *line, const ExpandContext *c, size_t argc,
                   const char *const *args) {
    TokenList t;
    Pipeline p;

    if (lex_expand(line, c, &t) != LEX_OK) {
        fprintf(stderr, "FAIL: could not lex: %s\n", line);
        failures++;
        return;
    }
    if (parse(&t, &p, NULL) != PARSE_OK) {
        fprintf(stderr, "FAIL: could not parse: %s\n", line);
        failures++;
        token_list_free(&t);
        return;
    }

    if (p.count != 1 || p.cmds[0].argc != argc) {
        fprintf(stderr, "FAIL: wrong argument count for: %s\n", line);
        failures++;
    } else {
        for (size_t i = 0; i < argc; i++) {
            if (strcmp(p.cmds[0].argv[i], args[i]) != 0) {
                fprintf(stderr, "FAIL: %s: arg %lu is '%s', expected '%s'\n", line,
                        (unsigned long)i, p.cmds[0].argv[i], args[i]);
                failures++;
            }
        }
    }
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_variables(void) {
    expect("echo $SHELLINC_A", &ctx, 2, (const char *[]){"echo", "hello"});
    expect("echo ${SHELLINC_A}world", &ctx, 2, (const char *[]){"echo", "helloworld"});
    expect("echo $SHELLINC_A-x", &ctx, 2, (const char *[]){"echo", "hello-x"});
    expect("echo \"$SHELLINC_A world\"", &ctx, 2, (const char *[]){"echo", "hello world"});
    expect("echo pre$SHELLINC_A$SHELLINC_A", &ctx, 2,
           (const char *[]){"echo", "prehellohello"});
}

static void test_quoting(void) {
    expect("echo '$SHELLINC_A'", &ctx, 2, (const char *[]){"echo", "$SHELLINC_A"});
    expect("echo \\$SHELLINC_A", &ctx, 2, (const char *[]){"echo", "$SHELLINC_A"});
    expect("echo \"\\$SHELLINC_A\"", &ctx, 2, (const char *[]){"echo", "$SHELLINC_A"});
    expect("echo \"'$SHELLINC_A'\"", &ctx, 2, (const char *[]){"echo", "'hello'"});
}

static void test_unset_variables(void) {
    TokenList t;
    Pipeline p;

    /* an unquoted word that expands to nothing disappears */
    expect("echo $SHELLINC_NOPE x", &ctx, 2, (const char *[]){"echo", "x"});
    /* a quoted one stays as an empty argument */
    expect("echo \"$SHELLINC_NOPE\" x", &ctx, 3, (const char *[]){"echo", "", "x"});
    /* attached to other text it just contributes nothing */
    expect("echo a$SHELLINC_NOPE", &ctx, 2, (const char *[]){"echo", "a"});

    /* a line that expands to nothing at all is an empty pipeline */
    CHECK(lex_expand("$SHELLINC_NOPE", &ctx, &t) == LEX_OK);
    CHECK(parse(&t, &p, NULL) == PARSE_OK);
    CHECK(p.count == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_status(void) {
    expect("echo $?", &ctx, 2, (const char *[]){"echo", "42"});
    expect("echo \"x$?y\"", &ctx, 2, (const char *[]){"echo", "x42y"});
    expect("echo '$?'", &ctx, 2, (const char *[]){"echo", "$?"});
}

static void test_literal_dollar(void) {
    expect("echo $", &ctx, 2, (const char *[]){"echo", "$"});
    expect("echo $5", &ctx, 2, (const char *[]){"echo", "$5"});
    expect("echo cost$", &ctx, 2, (const char *[]){"echo", "cost$"});
    expect("echo ${}", &ctx, 2, (const char *[]){"echo", "${}"});
    expect("echo ${SHELLINC_A", &ctx, 2, (const char *[]){"echo", "${SHELLINC_A"});
}

static void test_tilde(void) {
    expect("cd ~", &ctx, 2, (const char *[]){"cd", "/home/test"});
    expect("cd ~/src", &ctx, 2, (const char *[]){"cd", "/home/test/src"});
    expect("echo a~", &ctx, 2, (const char *[]){"echo", "a~"});
    expect("echo ~user", &ctx, 2, (const char *[]){"echo", "~user"});
    expect("echo '~'", &ctx, 2, (const char *[]){"echo", "~"});
    expect("echo \"~\"", &ctx, 2, (const char *[]){"echo", "~"});
    expect("echo ~", &no_home, 2, (const char *[]){"echo", "~"});
}

static void test_no_reparsing(void) {
    TokenList t;
    Pipeline p;

    /* special characters inside a value stay plain characters */
    expect("echo $SHELLINC_C", &ctx, 2, (const char *[]){"echo", "a|b"});
    /* no word splitting either */
    expect("echo $SHELLINC_B", &ctx, 2, (const char *[]){"echo", "two words"});

    CHECK(lex_expand("echo $SHELLINC_C", &ctx, &t) == LEX_OK);
    CHECK(parse(&t, &p, NULL) == PARSE_OK);
    CHECK(p.count == 1);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_redirect_target(void) {
    TokenList t;
    Pipeline p;

    CHECK(lex_expand("echo hi > $SHELLINC_OUT", &ctx, &t) == LEX_OK);
    CHECK(parse(&t, &p, NULL) == PARSE_OK);
    CHECK(p.count == 1);
    CHECK(p.cmds[0].out_file != NULL && strcmp(p.cmds[0].out_file, "out.txt") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_windows_paths_survive(void) {
    expect("cd C:\\Users\\ADMIN", &ctx, 2, (const char *[]){"cd", "C:\\Users\\ADMIN"});
}

static void test_plain_lex_does_not_expand(void) {
    TokenList t;
    Pipeline p;

    CHECK(lex("echo $SHELLINC_A ~", &t) == LEX_OK);
    CHECK(parse(&t, &p, NULL) == PARSE_OK);
    CHECK(p.count == 1 && p.cmds[0].argc == 3);
    CHECK(strcmp(p.cmds[0].argv[1], "$SHELLINC_A") == 0);
    CHECK(strcmp(p.cmds[0].argv[2], "~") == 0);
    pipeline_free(&p);
    token_list_free(&t);
}

static void test_errors_still_reported(void) {
    TokenList t;

    CHECK(lex_expand("echo \"$SHELLINC_A", &ctx, &t) == LEX_ERR_UNTERMINATED_QUOTE);
    CHECK(lex_expand("echo '$SHELLINC_A", &ctx, &t) == LEX_ERR_UNTERMINATED_QUOTE);
}

int main(void) {
    platform_setenv("SHELLINC_A", "hello");
    platform_setenv("SHELLINC_B", "two words");
    platform_setenv("SHELLINC_C", "a|b");
    platform_setenv("SHELLINC_OUT", "out.txt");
    platform_unsetenv("SHELLINC_NOPE");

    test_variables();
    test_quoting();
    test_unset_variables();
    test_status();
    test_literal_dollar();
    test_tilde();
    test_no_reparsing();
    test_redirect_target();
    test_windows_paths_survive();
    test_plain_lex_does_not_expand();
    test_errors_still_reported();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all expansion tests passed\n");
    return 0;
}