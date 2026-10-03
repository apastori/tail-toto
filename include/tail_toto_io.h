/*
 * tail_toto_io.h — raw read()/write() wrappers and platform I/O setup.
 */

#ifndef TAIL_TOTO_IO_H
#define TAIL_TOTO_IO_H

#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>

#define TAIL_TOTO_BUFSIZE 8192

/*
 * O_BINARY exists only on platforms with text-mode translation (Windows
 * runtimes, Cygwin); elsewhere binary mode is the only mode.
 */
#ifdef O_BINARY
#define TAIL_TOTO_O_BINARY O_BINARY
#else
#define TAIL_TOTO_O_BINARY 0
#endif

/*
 * Write all len bytes of buf to fd.
 *
 * Precondition: buf != NULL or len == 0.
 * Postcondition: returns 0 with every byte written, or -1 with errno set.
 * Retries on EINTR and continues after short writes. Never exits.
 */
int tail_toto_write_all(int fd, const void *buf, size_t len);

/*
 * read() that retries on EINTR.
 * Returns: bytes read (> 0), 0 on end of file, -1 on error with errno set.
 */
ssize_t tail_toto_read_retry(int fd, void *buf, size_t len);

/* Put stdin and stdout in binary mode (Win32 I/O builds); no-op elsewhere. */
void tail_toto_set_binary_stdio(void);

#endif /* TAIL_TOTO_IO_H */
