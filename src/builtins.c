#include "builtins.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

typedef int (*BuiltinFn)(const Command *c, FILE *out);

static int bi_echo(const Command *c, FILE *out) {
    size_t i = 1;
    int newline = 1;

    if (i < c->argc && strcmp(c->argv[i], "-n") == 0) {
        newline = 0;
        i++;
    }
    for (int first = 1; i < c->argc; i++, first = 0) {
        if (!first) {
            fputc(' ', out);
        }
        fputs(c->argv[i], out);
    }
    if (newline) {
        fputc('\n', out);
    }
    return 0;
}

static int bi_pwd(const Command *c, FILE *out) {
    char buf[4096];
    const char *err = platform_getcwd(buf, sizeof buf);

    (void)c;
    if (err) {
        fprintf(stderr, "shellinc: pwd: %s\n", err);
        return 1;
    }
    fprintf(out, "%s\n", buf);
    return 0;
}

static int bi_cd(const Command *c, FILE *out) {
    const char *target;
    const char *err;

    (void)out;
    if (c->argc > 2) {
        fprintf(stderr, "shellinc: cd: too many arguments\n");
        return 1;
    }
    if (c->argc == 2) {
        target = c->argv[1];
    } else {
        target = platform_home_dir();
        if (!target) {
            fprintf(stderr, "shellinc: cd: home directory not set\n");
            return 1;
        }
    }
    err = platform_chdir(target);
    if (err) {
        fprintf(stderr, "shellinc: cd: %s: %s\n", target, err);
        return 1;
    }
    return 0;
}

static int valid_name(const char *s) {
    if (!(isalpha((unsigned char)*s) || *s == '_')) {
        return 0;
    }
    for (s++; *s; s++) {
        if (!(isalnum((unsigned char)*s) || *s == '_')) {
            return 0;
        }
    }
    return 1;
}

static int bi_export(const Command *c, FILE *out) {
    int status = 0;

    (void)out;
    if (c->argc == 1) {
        fprintf(stderr, "usage: export NAME=value\n");
        return 1;
    }
    for (size_t i = 1; i < c->argc; i++) {
        const char *arg = c->argv[i];
        const char *eq = strchr(arg, '=');
        char name[256];
        size_t n = eq ? (size_t)(eq - arg) : strlen(arg);

        if (n >= sizeof name) {
            n = 0; /* too long: reported as invalid below */
        }
        memcpy(name, arg, n);
        name[n] = '\0';

        if (!valid_name(name)) {
            fprintf(stderr, "shellinc: export: '%s': not a valid identifier\n", arg);
            status = 1;
            continue;
        }
        if (eq) {
            const char *err = platform_setenv(name, eq + 1);
            if (err) {
                fprintf(stderr, "shellinc: export: %s: %s\n", name, err);
                status = 1;
            }
        }
    }
    return status;
}

static int bi_unset(const Command *c, FILE *out) {
    int status = 0;

    (void)out;
    for (size_t i = 1; i < c->argc; i++) {
        const char *err;

        if (!valid_name(c->argv[i])) {
            fprintf(stderr, "shellinc: unset: '%s': not a valid identifier\n", c->argv[i]);
            status = 1;
            continue;
        }
        err = platform_unsetenv(c->argv[i]);
        if (err) {
            fprintf(stderr, "shellinc: unset: %s: %s\n", c->argv[i], err);
            status = 1;
        }
    }
    return status;
}

static int bi_help(const Command *c, FILE *out) {
    (void)c;
    fputs("shellinc builtins:\n"
          "  cd [dir]           change directory (no argument: home directory)\n"
          "  pwd                print the current directory\n"
          "  echo [-n] [text]   print text\n"
          "  export NAME=value  set an environment variable\n"
          "  unset NAME         remove an environment variable\n"
          "  exit [n]           exit the shell with status n\n"
          "  help               show this message\n",
          out);
    return 0;
}

static void run_exit(const Command *c, int last_status, BuiltinResult *r) {
    char *end;
    long n;

    if (c->argc == 1) {
        r->status = last_status;
        r->exit_shell = 1;
        return;
    }
    n = strtol(c->argv[1], &end, 10);
    if (*c->argv[1] == '\0' || *end != '\0') {
        fprintf(stderr, "shellinc: exit: %s: numeric argument required\n", c->argv[1]);
        r->status = 2;
        r->exit_shell = 1;
        return;
    }
    if (c->argc > 2) {
        fprintf(stderr, "shellinc: exit: too many arguments\n");
        r->status = 1;
        return;
    }
    r->status = (int)(n & 0xFF);
    r->exit_shell = 1;
}

static const struct {
    const char *name;
    BuiltinFn fn;
} table[] = {
    {"cd", bi_cd},         {"pwd", bi_pwd},     {"echo", bi_echo}, {"export", bi_export},
    {"unset", bi_unset},   {"help", bi_help},
};

int builtin_is(const char *name) {
    if (strcmp(name, "exit") == 0) {
        return 1;
    }
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (strcmp(name, table[i].name) == 0) {
            return 1;
        }
    }
    return 0;
}

BuiltinResult builtin_run(const Command *c, FILE *out, int last_status) {
    BuiltinResult r = {0, 0};

    if (strcmp(c->argv[0], "exit") == 0) {
        run_exit(c, last_status, &r);
        return r;
    }
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (strcmp(c->argv[0], table[i].name) == 0) {
            r.status = table[i].fn(c, out);
            return r;
        }
    }
    r.status = 127;
    return r;
}