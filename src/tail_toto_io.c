/*
 * tail_toto_io.c — raw read()/write() wrappers and platform I/O setup.
 *
 * Chain of thought:
 *   Responsibility: the only place that talks to read(2)/write(2) directly
 *                   and the only production file with a platform guard.
 *   Syscalls:       read, write; _setmode on Win32 I/O builds.
 *   Heap:           none.
 *   Standard:       ISO C11 + POSIX.1-2008.
 *
 *   TAIL_TOTO_WIN32_IO is set by the Makefile when <io.h> provides
 *   _setmode(). Windows runtimes default stdin/stdout to text mode, which
 *   would translate CRLF and stop at Ctrl-Z; tail must pass bytes through
 *   unchanged, so both are switched to binary mode.
 */

#include <errno.h>
#include <unistd.h>

#ifdef TAIL_TOTO_WIN32_IO
#include <io.h>
#endif

#include "tail_toto_io.h"

int tail_toto_write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    ssize_t n;

    while (len > 0) {
        n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

ssize_t tail_toto_read_retry(int fd, void *buf, size_t len)
{
    ssize_t n;

    do {
        n = read(fd, buf, len);
    } while (n < 0 && errno == EINTR);
    return n;
}

void tail_toto_set_binary_stdio(void)
{
#ifdef TAIL_TOTO_WIN32_IO
    _setmode(STDIN_FILENO, _O_BINARY);
    _setmode(STDOUT_FILENO, _O_BINARY);
#endif
}
