#ifndef SHELLINC_PLATFORM_H
#define SHELLINC_PLATFORM_H

#include <stddef.h>

#include "parser.h"

/* Runs every command in the pipeline, connected by pipes and redirections,
 * waits for all of them, and returns the exit status of the last one.
 *   127  command not found
 *   126  command found but could not be run
 *   128+N  killed by signal N (POSIX only)
 * Returns 0 for an empty pipeline. */
int platform_run_pipeline(const Pipeline *p);

/* Helpers for the shell's own state. Functions returning const char * return
 * NULL on success, or a message describing the failure (valid until the next
 * platform call). */
const char *platform_chdir(const char *path);
const char *platform_getcwd(char *buf, size_t size);
const char *platform_setenv(const char *name, const char *value);
const char *platform_unsetenv(const char *name);

/* Home directory (HOME, or USERPROFILE on Windows); NULL if not set. */
const char *platform_home_dir(void);

#endif