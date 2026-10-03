/*
 * main.c — tail-toto entry point.
 *
 * Chain of thought:
 *   Responsibility: dispatch --help / --version, parse arguments, hand off
 *                   to tail_toto_run(), and return its exit code. Nothing
 *                   else lives here: no file I/O, no tail logic.
 *   Syscalls:       none directly.
 *   Heap:           none.
 *   Standard:       ISO C11.
 */

#include "tail_toto.h"
#include "tail_toto_cli.h"
#include "tail_toto_run.h"

int main(int argc, char **argv)
{
    struct tail_toto_opts opts;

    switch (scan_meta_flags(argc, argv)) {
    case TAIL_TOTO_META_HELP:
        return print_help() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    case TAIL_TOTO_META_VERSION:
        return print_version() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    case TAIL_TOTO_META_NONE:
        break;
    }

    if (tail_toto_parse_args(argc, argv, &opts) != 0) {
        return TAIL_TOTO_EXIT_ERR;
    }

    return tail_toto_run(&opts);
}
