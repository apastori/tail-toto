/*
 * main.c — tail-toto entry point.
 *
 * Chain of thought:
 *   Responsibility: parse arguments, dispatch --help / --version, print the
 *                   GNU warnings for follow-only options, stop early when
 *                   -n 0 / -c 0 means nothing can be printed, otherwise hand
 *                   off to tail_toto_run() and return its exit code. Nothing
 *                   else lives here: no file I/O, no tail logic.
 *   Syscalls:       none directly.
 *   Heap:           none.
 *   Standard:       ISO C11.
 */

#include "tail_toto.h"
#include "tail_toto_cli.h"
#include "tail_toto_emit.h"
#include "tail_toto_run.h"

int main(int argc, char **argv)
{
    struct tail_toto_opts opts;
    enum tail_toto_parse_result result;

    result = tail_toto_parse_args(argc, argv, &opts);

    if (result == TAIL_TOTO_PARSE_ERROR) {
        return TAIL_TOTO_EXIT_ERR;
    }
    if (result == TAIL_TOTO_PARSE_HELP) {
        return print_help() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    }
    if (result == TAIL_TOTO_PARSE_VERSION) {
        return print_version() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    }

    /* Without follow mode these options only earn a warning, as in GNU */
    if (opts.retry) {
        tail_toto_emit_retry_warning();
    }
    if (opts.pid_given) {
        tail_toto_emit_pid_warning();
    }

    /* GNU opens nothing when nothing can be printed: no headers, no errors */
    if (opts.count == 0 && (opts.mode == TAIL_TOTO_MODE_LINES_LAST
                            || opts.mode == TAIL_TOTO_MODE_BYTES_LAST)) {
        return TAIL_TOTO_EXIT_OK;
    }

    return tail_toto_run(&opts);
}
