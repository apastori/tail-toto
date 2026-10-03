/*
 * tail_toto.h — version, options, and exit codes for tail-toto.
 */

#ifndef TAIL_TOTO_H
#define TAIL_TOTO_H

#include <stdint.h>

#define TAIL_TOTO_VERSION_STRING "1.0.0"
#define TAIL_TOTO_PROGRAM_NAME "tail-toto"
#define TAIL_TOTO_DEFAULT_COUNT 10u
#define TAIL_TOTO_STDIN_NAME "standard input"

enum tail_toto_exit {
    TAIL_TOTO_EXIT_OK = 0,
    TAIL_TOTO_EXIT_ERR = 1
};

enum tail_toto_mode {
    TAIL_TOTO_MODE_LINES_LAST,   /* -n N   (default, N = 10) */
    TAIL_TOTO_MODE_LINES_FROM,   /* -n +N */
    TAIL_TOTO_MODE_BYTES_LAST,   /* -c N   */
    TAIL_TOTO_MODE_BYTES_FROM    /* -c +N  */
};

struct tail_toto_opts {
    enum tail_toto_mode mode;
    uintmax_t count;
    int headers;                 /* -1 auto, 0 = -q, 1 = -v */
    int nfiles;
    char **files;                /* points into argv; never copied */
};

#endif /* TAIL_TOTO_H */
