/*
 * tail_toto_run.h — per-operand driver for tail-toto.
 */

#ifndef TAIL_TOTO_RUN_H
#define TAIL_TOTO_RUN_H

#include "tail_toto.h"

/*
 * Process every operand in opts->files in order and write the selected tail
 * of each to stdout.
 *
 * Precondition: opts was filled by tail_toto_parse_args(); nfiles >= 1.
 * Returns: TAIL_TOTO_EXIT_OK if every input was processed, otherwise
 *          TAIL_TOTO_EXIT_ERR. Exits the process on a write error.
 */
int tail_toto_run(const struct tail_toto_opts *opts);

#endif /* TAIL_TOTO_RUN_H */
