#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Runs in the forked child and never returns. */
static void run_child(const Command *c, int in_fd, int pipe_r, int pipe_w) {
    int err;

    if (in_fd != -1) {
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }
    if (pipe_w != -1) {
        dup2(pipe_w, STDOUT_FILENO);
        close(pipe_w);
    }
    if (pipe_r != -1) { /* the read end of our own output pipe belongs to the next command */
        close(pipe_r);
    }

    if (c->in_file) { /* an explicit < overrides the pipe */
        int fd = open(c->in_file, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "shellinc: %s: %s\n", c->in_file, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->out_file) { /* an explicit > or >> overrides the pipe */
        int flags = O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC);
        int fd = open(c->out_file, flags, 0666);
        if (fd == -1) {
            fprintf(stderr, "shellinc: %s: %s\n", c->out_file, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    execvp(c->argv[0], c->argv);

    err = errno;
    fprintf(stderr, "shellinc: %s: %s\n", c->argv[0],
            err == ENOENT ? "command not found" : strerror(err));
    _exit(err == ENOENT ? 127 : 126);
}

static int wait_for(pid_t pid) {
    int ws;

    while (waitpid(pid, &ws, 0) == -1) {
        if (errno != EINTR) {
            return 1;
        }
    }
    if (WIFEXITED(ws)) {
        return WEXITSTATUS(ws);
    }
    if (WIFSIGNALED(ws)) {
        return 128 + WTERMSIG(ws);
    }
    return 1;
}

int platform_run_pipeline(const Pipeline *p) {
    pid_t *pids;
    size_t started = 0;
    int prev_read = -1;
    int failed = 0;
    int status;

    if (p->count == 0) {
        return 0;
    }

    pids = malloc(p->count * sizeof *pids);
    if (!pids) {
        fprintf(stderr, "shellinc: out of memory\n");
        return 1;
    }

    fflush(NULL); /* otherwise buffered output would be duplicated in each child */

    for (size_t i = 0; i < p->count; i++) {
        int fds[2] = {-1, -1};
        pid_t pid;

        if (i + 1 < p->count && pipe(fds) == -1) {
            perror("shellinc: pipe");
            failed = 1;
            break;
        }

        pid = fork();
        if (pid == -1) {
            perror("shellinc: fork");
            if (fds[0] != -1) {
                close(fds[0]);
                close(fds[1]);
            }
            failed = 1;
            break;
        }
        if (pid == 0) {
            run_child(&p->cmds[i], prev_read, fds[0], fds[1]);
        }

        pids[started++] = pid;
        if (prev_read != -1) {
            close(prev_read);
        }
        if (fds[1] != -1) {
            close(fds[1]);
        }
        prev_read = fds[0];
    }
    if (prev_read != -1) {
        close(prev_read);
    }

    status = failed ? 1 : 0;
    for (size_t i = 0; i < started; i++) {
        int st = wait_for(pids[i]);
        if (i == started - 1 && !failed) {
            status = st;
        }
    }

    free(pids);
    return status;
}