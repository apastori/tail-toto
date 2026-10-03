/*
 * tail_toto_cli.h — command-line flags, argument parsing, help and version.
 */

#ifndef TAIL_TOTO_CLI_H
#define TAIL_TOTO_CLI_H

#include <stdint.h>

#include "tail_toto.h"

#define TAIL_TOTO_ARG_HELP "--help"
#define TAIL_TOTO_ARG_HELP_SHORT "--h"
#define TAIL_TOTO_ARG_VERSION "--version"
#define TAIL_TOTO_ARG_VERSION_SHORT "--v"
#define TAIL_TOTO_ARG_LINES "--lines"
#define TAIL_TOTO_ARG_LINES_SHORT "-n"
#define TAIL_TOTO_ARG_BYTES "--bytes"
#define TAIL_TOTO_ARG_BYTES_SHORT "-c"
#define TAIL_TOTO_ARG_QUIET "--quiet"
#define TAIL_TOTO_ARG_SILENT "--silent"
#define TAIL_TOTO_ARG_QUIET_SHORT "-q"
#define TAIL_TOTO_ARG_VERBOSE "--verbose"
#define TAIL_TOTO_ARG_VERBOSE_SHORT "-v"
#define TAIL_TOTO_ARG_END "--"
#define TAIL_TOTO_ARG_STDIN "-"

enum tail_toto_meta {
    TAIL_TOTO_META_NONE,
    TAIL_TOTO_META_HELP,
    TAIL_TOTO_META_VERSION
};

/*
 * Scan argv[1..] up to the first "--" for help and version flags.
 * Returns: TAIL_TOTO_META_HELP if any help flag is present (wins over
 *          version), TAIL_TOTO_META_VERSION if only a version flag is
 *          present, otherwise TAIL_TOTO_META_NONE.
 */
enum tail_toto_meta scan_meta_flags(int argc, char **argv);

/*
 * Parse options and operands into *opts.
 *
 * Operands are compacted in place to the front of argv[1..] so that
 * opts->files can point into argv without heap allocation. With no
 * operands, opts->files names a single "-" (stdin).
 *
 * Returns: 0 on success; -1 after emitting a diagnostic on usage errors.
 */
int tail_toto_parse_args(int argc, char **argv, struct tail_toto_opts *opts);

/*
 * Parse a count of the form [+|-]DIGITS.
 *
 * Postcondition on success: *out holds the value (saturated to UINTMAX_MAX
 * on overflow); *from_start is 1 when a leading '+' was present, else 0.
 * Returns: 0 on success, -1 on invalid input. Emits no diagnostics.
 */
int tail_toto_parse_count(const char *s, uintmax_t *out, int *from_start);

/* Print usage to stdout. Returns 0, or -1 after a write-error diagnostic. */
int print_help(void);

/* Print "tail-toto <version>\n" to stdout. Same return contract as above. */
int print_version(void);

#endif /* TAIL_TOTO_CLI_H */
