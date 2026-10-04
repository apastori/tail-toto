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
    tail_toto_meta_t meta;

    meta = scan_meta_flags(argc, argv);

    /* Handle meta flags */
    // Help Meta Flag
    if (meta == TAIL_TOTO_META_HELP) {
        return print_help() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    } 
    // Version Meta Flag
    if (meta == TAIL_TOTO_META_VERSION) {
        return print_version() == 0 ? TAIL_TOTO_EXIT_OK : TAIL_TOTO_EXIT_ERR;
    } 

    /* Handle main logic */
    // Parse arguments
    if (tail_toto_parse_args(argc, argv, &opts) != 0) {
        return TAIL_TOTO_EXIT_ERR;
    }

    /* Run the program */
    return tail_toto_run(&opts);
}
