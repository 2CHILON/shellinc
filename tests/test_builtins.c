#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "lexer.h"
#include "parser.h"
#include "platform.h"

#define TMP_FILE "test_builtins.tmp"

static int failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                     \
        }                                                                   \
    } while (0)

/* Runs a single builtin command line and captures its normal output in out. */
static BuiltinResult run(const char *line, char *out, size_t cap, int last_status) {
    BuiltinResult r = {-1, 0};
    TokenList t;
    Pipeline p;
    FILE *f;
    size_t n;

    out[0] = '\0';
    if (lex(line, &t) != LEX_OK) {
        failures++;
        return r;
    }
    if (parse(&t, &p, NULL) != PARSE_OK) {
        token_list_free(&t);
        failures++;
        return r;
    }
    f = fopen(TMP_FILE, "w+");
    if (!f) {
        pipeline_free(&p);
        token_list_free(&t);
        failures++;
        return r;
    }

    r = builtin_run(&p.cmds[0], f, last_status);

    rewind(f);
    n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    remove(TMP_FILE);
    pipeline_free(&p);
    token_list_free(&t);
    return r;
}

static void test_is_builtin(void) {
    CHECK(builtin_is("cd"));
    CHECK(builtin_is("pwd"));
    CHECK(builtin_is("echo"));
    CHECK(builtin_is("export"));
    CHECK(builtin_is("unset"));
    CHECK(builtin_is("exit"));
    CHECK(builtin_is("help"));
    CHECK(!builtin_is("ls"));
    CHECK(!builtin_is("git"));
    CHECK(builtin_is("clear"));
    CHECK(builtin_is("cls"));
}

static void test_echo(void) {
    char out[256];
    BuiltinResult r;

    r = run("echo hello   world", out, sizeof out, 0);
    CHECK(r.status == 0);
    CHECK(!r.exit_shell);
    CHECK(strcmp(out, "hello world\n") == 0);

    run("echo -n hi", out, sizeof out, 0);
    CHECK(strcmp(out, "hi") == 0);

    run("echo \"a  b\" c", out, sizeof out, 0);
    CHECK(strcmp(out, "a  b c\n") == 0);

    run("echo", out, sizeof out, 0);
    CHECK(strcmp(out, "\n") == 0);
}

static void test_exit(void) {
    char out[64];
    BuiltinResult r;

    r = run("exit", out, sizeof out, 7);
    CHECK(r.exit_shell);
    CHECK(r.status == 7);

    r = run("exit 3", out, sizeof out, 0);
    CHECK(r.exit_shell);
    CHECK(r.status == 3);

    r = run("exit 256", out, sizeof out, 0);
    CHECK(r.exit_shell);
    CHECK(r.status == 0);

    r = run("exit abc", out, sizeof out, 0);
    CHECK(r.exit_shell);
    CHECK(r.status == 2);

    r = run("exit 1 2", out, sizeof out, 0);
    CHECK(!r.exit_shell);
    CHECK(r.status == 1);
}

static void test_env(void) {
    char out[64];
    BuiltinResult r;
    const char *v;

    r = run("export SHELLINC_TEST=hello", out, sizeof out, 0);
    CHECK(r.status == 0);
    v = getenv("SHELLINC_TEST");
    CHECK(v != NULL && strcmp(v, "hello") == 0);

    r = run("unset SHELLINC_TEST", out, sizeof out, 0);
    CHECK(r.status == 0);
    CHECK(getenv("SHELLINC_TEST") == NULL);

    r = run("export 1BAD=x", out, sizeof out, 0);
    CHECK(r.status == 1);

    r = run("export BAD-NAME=x", out, sizeof out, 0);
    CHECK(r.status == 1);

    r = run("unset 1BAD", out, sizeof out, 0);
    CHECK(r.status == 1);

    r = run("export", out, sizeof out, 0);
    CHECK(r.status == 1);
}

static void test_cd_pwd(void) {
    char start[4096];
    char now[4096];
    char out[4200];
    BuiltinResult r;
    size_t n;

    if (platform_getcwd(start, sizeof start) != NULL) {
        CHECK(0 && "getcwd failed");
        return;
    }

    r = run("cd ..", out, sizeof out, 0);
    CHECK(r.status == 0);
    CHECK(platform_getcwd(now, sizeof now) == NULL);
    CHECK(strcmp(start, now) != 0);

    r = run("pwd", out, sizeof out, 0);
    CHECK(r.status == 0);
    n = strlen(out);
    if (n > 0 && out[n - 1] == '\n') {
        out[n - 1] = '\0';
    }
    CHECK(strcmp(out, now) == 0);

    CHECK(platform_chdir(start) == NULL); /* restore the original directory */

    r = run("cd definitely-not-a-real-directory", out, sizeof out, 0);
    CHECK(r.status == 1);

    r = run("cd a b", out, sizeof out, 0);
    CHECK(r.status == 1);
}

static void test_help(void) {
    char out[1024];
    BuiltinResult r = run("help", out, sizeof out, 0);

    CHECK(r.status == 0);
    CHECK(strstr(out, "cd") != NULL);
    CHECK(strstr(out, "exit") != NULL);
}
static void test_clear(void) {
    char out[64];
    BuiltinResult r;

    /* output goes to a file here, so nothing is written and no screen is touched */
    r = run("clear", out, sizeof out, 0);
    CHECK(r.status == 0);
    CHECK(!r.exit_shell);
    CHECK(strcmp(out, "") == 0);

    r = run("cls", out, sizeof out, 0);
    CHECK(r.status == 0);
    CHECK(strcmp(out, "") == 0);
}
int main(void) {
    test_is_builtin();
    test_echo();
    test_exit();
    test_env();
    test_cd_pwd();
    test_help();
    test_clear();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all builtin tests passed\n");
    return 0;
}