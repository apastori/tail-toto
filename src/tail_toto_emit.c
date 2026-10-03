/*
 * tail_toto_emit.c — stderr diagnostics.
 *
 * Chain of thought:
 *   Responsibility: format every user-visible error message in one place,
 *                   using GNU tail wording with the tail-toto prefix.
 *   Syscalls:       none directly; stdio on stderr only (never stdout, so it
 *                   cannot interleave with raw write() output).
 *   Heap:           none.
 *   Standard:       ISO C11.
 *
 *   errno is captured on entry because fprintf() may clobber it before
 *   strerror() is evaluated.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "tail_toto.h"
#include "tail_toto_emit.h"

#define TAIL_TOTO_HINT \
    "Try '" TAIL_TOTO_PROGRAM_NAME " --help' for more information.\n"

void tail_toto_emit_error(const char *context)
{
    int err = errno;

    fprintf(stderr, "%s: %s: %s\n", TAIL_TOTO_PROGRAM_NAME, context,
            strerror(err));
}

void tail_toto_emit_open_error(const char *name)
{
    int err = errno;

    fprintf(stderr, "%s: cannot open '%s' for reading: %s\n",
            TAIL_TOTO_PROGRAM_NAME, name, strerror(err));
}

void tail_toto_emit_read_error(const char *name)
{
    int err = errno;

    fprintf(stderr, "%s: error reading '%s': %s\n", TAIL_TOTO_PROGRAM_NAME,
            name, strerror(err));
}

void tail_toto_emit_write_error(void)
{
    int err = errno;

    fprintf(stderr, "%s: write error: %s\n", TAIL_TOTO_PROGRAM_NAME,
            strerror(err));
}

void tail_toto_emit_invalid_count(int bytes, const char *arg)
{
    fprintf(stderr, "%s: invalid number of %s: '%s'\n",
            TAIL_TOTO_PROGRAM_NAME, bytes ? "bytes" : "lines", arg);
}

void tail_toto_emit_bad_option(const char *long_arg, char short_opt)
{
    if (long_arg != NULL) {
        fprintf(stderr, "%s: unrecognized option '%s'\n",
                TAIL_TOTO_PROGRAM_NAME, long_arg);
    } else {
        fprintf(stderr, "%s: invalid option -- '%c'\n",
                TAIL_TOTO_PROGRAM_NAME, short_opt);
    }
    fputs(TAIL_TOTO_HINT, stderr);
}

void tail_toto_emit_missing_arg(const char *long_name, char short_opt)
{
    if (long_name != NULL) {
        fprintf(stderr, "%s: option '%s' requires an argument\n",
                TAIL_TOTO_PROGRAM_NAME, long_name);
    } else {
        fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                TAIL_TOTO_PROGRAM_NAME, short_opt);
    }
    fputs(TAIL_TOTO_HINT, stderr);
}
