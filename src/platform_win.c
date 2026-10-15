#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

static void report(const char *what, DWORD err) {
    char msg[256];
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, err, 0, msg, (DWORD)sizeof msg, NULL);
    if (n == 0) {
        snprintf(msg, sizeof msg, "error %lu", (unsigned long)err);
    } else {
        while (n > 0 && (msg[n - 1] == '\r' || msg[n - 1] == '\n' ||
                         msg[n - 1] == ' ' || msg[n - 1] == '.')) {
            n--;
        }
        msg[n] = '\0';
    }
    fprintf(stderr, "shellinc: %s: %s\n", what, msg);
}

/* Appends arg to out using the quoting rules CreateProcess programs expect
 * (the CommandLineToArgvW rules). Returns the number of bytes written. */
static size_t append_quoted(char *out, const char *arg) {
    size_t n = 0;

    if (*arg && !strpbrk(arg, " \t\"")) { /* plain word, no quoting needed */
        size_t len = strlen(arg);
        memcpy(out, arg, len);
        return len;
    }

    out[n++] = '"';
    for (const char *p = arg;; p++) {
        size_t backslashes = 0;
        while (*p == '\\') {
            backslashes++;
            p++;
        }
        if (*p == '\0') { /* backslashes before the closing quote must be doubled */
            for (size_t k = 0; k < backslashes * 2; k++) {
                out[n++] = '\\';
            }
            break;
        }
        if (*p == '"') { /* backslashes before a quote are doubled, the quote is escaped */
            for (size_t k = 0; k < backslashes * 2 + 1; k++) {
                out[n++] = '\\';
            }
            out[n++] = '"';
        } else {
            for (size_t k = 0; k < backslashes; k++) {
                out[n++] = '\\';
            }
            out[n++] = *p;
        }
    }
    out[n++] = '"';
    return n;
}

static char *build_cmdline(const Command *c) {
    size_t cap = 1;
    size_t n = 0;
    char *buf;

    for (size_t i = 0; i < c->argc; i++) {
        cap += strlen(c->argv[i]) * 2 + 4;
    }
    buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    for (size_t i = 0; i < c->argc; i++) {
        if (i > 0) {
            buf[n++] = ' ';
        }
        n += append_quoted(buf + n, c->argv[i]);
    }
    buf[n] = '\0';
    return buf;
}

static void make_inheritable(HANDLE h) {
    if (h != NULL && h != INVALID_HANDLE_VALUE) {
        SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
}

static HANDLE open_input(const char *path) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
}

static HANDLE open_output(const char *path, int append) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    return CreateFileA(path, append ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ,
                       &sa, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* Starts one command. in/out are pipe ends, or INVALID_HANDLE_VALUE for the
 * console. Returns 0 on success (process handle in *proc), otherwise the exit
 * status to report after printing an error. */
static int spawn(const Command *c, HANDLE in, HANDLE out, HANDLE *proc) {
    HANDLE file_in = INVALID_HANDLE_VALUE;
    HANDLE file_out = INVALID_HANDLE_VALUE;
    HANDLE h_in = in;
    HANDLE h_out = out;
    char *cmdline = NULL;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    int result = 0;

    if (c->in_file) { /* an explicit < overrides the pipe */
        file_in = open_input(c->in_file);
        if (file_in == INVALID_HANDLE_VALUE) {
            report(c->in_file, GetLastError());
            result = 1;
            goto done;
        }
        h_in = file_in;
    }
    if (c->out_file) { /* an explicit > or >> overrides the pipe */
        file_out = open_output(c->out_file, c->append);
        if (file_out == INVALID_HANDLE_VALUE) {
            report(c->out_file, GetLastError());
            result = 1;
            goto done;
        }
        h_out = file_out;
    }

    if (h_in == INVALID_HANDLE_VALUE) {
        h_in = GetStdHandle(STD_INPUT_HANDLE);
    }
    if (h_out == INVALID_HANDLE_VALUE) {
        h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    cmdline = build_cmdline(c);
    if (!cmdline) {
        fprintf(stderr, "shellinc: out of memory\n");
        result = 1;
        goto done;
    }

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = h_in;
    si.hStdOutput = h_out;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /* Only the handles this child needs are made inheritable, so other
     * children never hold the ends of a pipe they do not use. */
    make_inheritable(si.hStdInput);
    make_inheritable(si.hStdOutput);
    make_inheritable(si.hStdError);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            fprintf(stderr, "shellinc: %s: command not found\n", c->argv[0]);
            result = 127;
        } else {
            report(c->argv[0], err);
            result = 126;
        }
        goto done;
    }
    CloseHandle(pi.hThread);
    *proc = pi.hProcess;

done:
    free(cmdline);
    if (file_in != INVALID_HANDLE_VALUE) {
        CloseHandle(file_in);
    }
    if (file_out != INVALID_HANDLE_VALUE) {
        CloseHandle(file_out);
    }
    return result;
}

int platform_run_pipeline(const Pipeline *p) {
    HANDLE *procs;
    HANDLE prev_read = INVALID_HANDLE_VALUE;
    size_t started = 0;
    int status = 0;
    int last_started = 0;

    if (p->count == 0) {
        return 0;
    }

    procs = malloc(p->count * sizeof *procs);
    if (!procs) {
        fprintf(stderr, "shellinc: out of memory\n");
        return 1;
    }

    fflush(NULL);

    for (size_t i = 0; i < p->count; i++) {
        int is_last = (i + 1 == p->count);
        HANDLE pipe_r = INVALID_HANDLE_VALUE;
        HANDLE pipe_w = INVALID_HANDLE_VALUE;
        HANDLE proc = NULL;
        int rc;

        /* Pipe handles are created non-inheritable on purpose. */
        if (!is_last && !CreatePipe(&pipe_r, &pipe_w, NULL, 0)) {
            report("pipe", GetLastError());
            status = 1;
            break;
        }

        rc = spawn(&p->cmds[i], prev_read, pipe_w, &proc);
        if (rc == 0) {
            procs[started++] = proc;
            if (is_last) {
                last_started = 1;
            }
        } else {
            status = rc;
        }

        /* The parent must close its copies so readers see end-of-file. */
        if (prev_read != INVALID_HANDLE_VALUE) {
            CloseHandle(prev_read);
        }
        if (pipe_w != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_w);
        }
        prev_read = pipe_r;
    }
    if (prev_read != INVALID_HANDLE_VALUE) {
        CloseHandle(prev_read);
    }

    for (size_t i = 0; i < started; i++) {
        WaitForSingleObject(procs[i], INFINITE);
        if (last_started && i == started - 1) {
            DWORD code = 0;
            GetExitCodeProcess(procs[i], &code);
            status = (int)code;
        }
        CloseHandle(procs[i]);
    }

    free(procs);
    return status;
}