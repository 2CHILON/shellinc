#ifndef SHELLINC_PLATFORM_H
#define SHELLINC_PLATFORM_H

#include "parser.h"

/* Runs every command in the pipeline, connected by pipes and redirections,
 * waits for all of them, and returns the exit status of the last one.
 *   127  command not found
 *   126  command found but could not be run
 *   128+N  killed by signal N (POSIX only)
 * Returns 0 for an empty pipeline. */
int platform_run_pipeline(const Pipeline *p);

#endif