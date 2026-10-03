/*
 * tail_toto_run.c — per-operand driver.
 *
 * Chain of thought:
 *   Responsibility: open each operand in order, print "==> name <==" headers
 *                   when required, delegate to tail_toto_tail_fd(), and
 *                   aggregate the exit status. Open/read errors are reported
 *                   and processing continues; a write error is fatal because
 *                   no further output can succeed.
 *   Syscalls:       open, close, write (via tail_toto_io).
 *   Heap:           none.
 *   Standard:       ISO C11 + POSIX.1-2008.
 *
 *   Headers are written only after a successful open, and every header but
 *   the first is preceded by a blank line, matching GNU tail.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tail_toto.h"
#include "tail_toto_cli.h"
#include "tail_toto_emit.h"
#include "tail_toto_io.h"
#include "tail_toto_run.h"
#include "tail_toto_tail.h"

static void write_or_die(const char *s, size_t len)
{
    if (tail_toto_write_all(STDOUT_FILENO, s, len) != 0) {
        tail_toto_emit_write_error();
        exit(TAIL_TOTO_EXIT_ERR);
    }
}

static void write_header(const char *name, int first)
{
    static const char open_first[] = "==> ";
    static const char open_next[] = "\n==> ";
    static const char close_mark[] = " <==\n";

    if (first) {
        write_or_die(open_first, sizeof open_first - 1);
    } else {
        write_or_die(open_next, sizeof open_next - 1);
    }
    write_or_die(name, strlen(name));
    write_or_die(close_mark, sizeof close_mark - 1);
}

int tail_toto_run(const struct tail_toto_opts *opts)
{
    int status = TAIL_TOTO_EXIT_OK;
    int show = opts->headers == 1 || (opts->headers == -1 && opts->nfiles > 1);
    int first_header = 1;
    int is_stdin;
    const char *name;
    int fd;
    int i;

    tail_toto_set_binary_stdio();

    for (i = 0; i < opts->nfiles; i++) {
        is_stdin = strcmp(opts->files[i], TAIL_TOTO_ARG_STDIN) == 0;
        name = is_stdin ? TAIL_TOTO_STDIN_NAME : opts->files[i];
        fd = is_stdin ? STDIN_FILENO
                      : open(opts->files[i], O_RDONLY | TAIL_TOTO_O_BINARY);
        if (fd < 0) {
            tail_toto_emit_open_error(name);
            status = TAIL_TOTO_EXIT_ERR;
            continue;
        }

        if (show) {
            write_header(name, first_header);
            first_header = 0;
        }

        switch (tail_toto_tail_fd(fd, STDOUT_FILENO, opts)) {
        case TAIL_TOTO_OK:
            break;
        case TAIL_TOTO_READ_ERR:
            tail_toto_emit_read_error(name);
            status = TAIL_TOTO_EXIT_ERR;
            break;
        case TAIL_TOTO_WRITE_ERR:
            tail_toto_emit_write_error();
            exit(TAIL_TOTO_EXIT_ERR);
        }

        if (!is_stdin && close(fd) != 0) {
            tail_toto_emit_error(name);
            status = TAIL_TOTO_EXIT_ERR;
        }
    }
    return status;
}
