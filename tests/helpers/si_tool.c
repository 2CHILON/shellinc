/* Tiny helper program for the end-to-end tests. It behaves identically on
 * Linux and Windows, so test scripts do not depend on platform tools. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define SI_GETCWD _getcwd
#define SI_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define SI_GETCWD getcwd
#define SI_MKDIR(path) mkdir(path, 0777)
#endif

/* si_tool cat: copies stdin to stdout */
static int cmd_cat(void) {
    int c;

    while ((c = getchar()) != EOF) {
        putchar(c);
    }
    return 0;
}

/* si_tool args A B C: prints <A>, <B>, <C> on separate lines */
static int cmd_args(int argc, char **argv) {
    for (int i = 2; i < argc; i++) {
        printf("<%s>\n", argv[i]);
    }
    return 0;
}

/* si_tool exit N: exits with status N */
static int cmd_exit(int argc, char **argv) {
    return argc > 2 ? atoi(argv[2]) : 0;
}

/* si_tool getenv NAME: prints the value, or "unset" */
static int cmd_getenv(int argc, char **argv) {
    const char *value = argc > 2 ? getenv(argv[2]) : NULL;

    printf("%s\n", value ? value : "unset");
    return 0;
}

/* si_tool mkdir DIR */
static int cmd_mkdir(int argc, char **argv) {
    return (argc > 2 && SI_MKDIR(argv[2]) == 0) ? 0 : 1;
}

/* si_tool cwdname: prints the last component of the current directory */
static int cmd_cwdname(void) {
    char buf[4096];
    const char *name = buf;

    if (!SI_GETCWD(buf, sizeof buf)) {
        return 1;
    }
    for (const char *p = buf; *p; p++) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }
    printf("%s\n", name);
    return 0;
}

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "";
    int rc;

    if (strcmp(cmd, "cat") == 0) {
        rc = cmd_cat();
    } else if (strcmp(cmd, "args") == 0) {
        rc = cmd_args(argc, argv);
    } else if (strcmp(cmd, "exit") == 0) {
        rc = cmd_exit(argc, argv);
    } else if (strcmp(cmd, "getenv") == 0) {
        rc = cmd_getenv(argc, argv);
    } else if (strcmp(cmd, "mkdir") == 0) {
        rc = cmd_mkdir(argc, argv);
    } else if (strcmp(cmd, "cwdname") == 0) {
        rc = cmd_cwdname();
    } else {
        fprintf(stderr, "si_tool: unknown command '%s'\n", cmd);
        return 2;
    }

    fflush(stdout);
    return rc;
}