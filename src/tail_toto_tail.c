/*
 * tail_toto_tail.c — tail algorithms over file descriptors.
 *
 * Chain of thought:
 *   Responsibility: given an input fd, copy the selected tail to an output
 *                   fd. Returns a status instead of exiting or printing, so
 *                   unit tests can drive every path directly.
 *   Path choice:    fstat() reports a regular file and lseek() works ->
 *                   seekable path: scan backwards from the end with a stack
 *                   buffer, then stream forward from the start offset.
 *                   Anything else (pipes, terminals, a regular file that
 *                   reports no data, e.g. under /proc) -> stream path.
 *   Syscalls:       fstat, lseek, read, write (via tail_toto_io).
 *   Heap:           only on the stream path for "last N" modes: a list of
 *                   fixed-size chunks (lines) or a growing ring buffer
 *                   (bytes). Freed on every return path.
 *   Standard:       ISO C11 + POSIX.1-2008 (_FILE_OFFSET_BITS=64 off_t).
 *
 *   A final '\n' terminates the last line rather than starting a new one;
 *   a final line without '\n' still counts as a line and is copied as-is.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tail_toto_io.h"
#include "tail_toto_tail.h"

struct line_chunk {
    struct line_chunk *next;
    size_t len;
    uintmax_t nlines;
    char data[TAIL_TOTO_BUFSIZE];
};

/* Copy everything from the current offset of in_fd to EOF. */
static enum tail_toto_status copy_to_eof(int in_fd, int out_fd)
{
    char buf[TAIL_TOTO_BUFSIZE];
    ssize_t n;

    for (;;) {
        n = tail_toto_read_retry(in_fd, buf, sizeof buf);
        if (n < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        if (n == 0) {
            return TAIL_TOTO_OK;
        }
        if (tail_toto_write_all(out_fd, buf, (size_t)n) != 0) {
            return TAIL_TOTO_WRITE_ERR;
        }
    }
}

/*
 * Decide whether fd can be handled by the seekable path.
 *
 * Postcondition when 1 is returned: *start is the initial offset, *end the
 * file size, *end > *start, and the file offset is unspecified.
 * When 0 is returned the file offset is unchanged.
 */
static int probe_seekable(int fd, off_t *start, off_t *end)
{
    struct stat st;

    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }
    *start = lseek(fd, 0, SEEK_CUR);
    if (*start < 0) {
        return 0;
    }
    *end = lseek(fd, 0, SEEK_END);
    if (*end <= *start) {
        lseek(fd, *start, SEEK_SET);
        return 0;
    }
    return 1;
}

/* Read up to len bytes, retrying short reads until len or EOF. */
static ssize_t read_full(int fd, char *buf, size_t len)
{
    size_t got = 0;
    ssize_t n;

    while (got < len) {
        n = tail_toto_read_retry(fd, buf + got, len - got);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            break;
        }
        got += (size_t)n;
    }
    return (ssize_t)got;
}

/*
 * Seekable, last N lines (N > 0): walk backwards in TAIL_TOTO_BUFSIZE
 * chunks counting '\n' until the Nth one before the final line is found.
 */
static enum tail_toto_status seek_lines_last(int in_fd, int out_fd,
                                             off_t start, off_t end,
                                             uintmax_t n)
{
    char buf[TAIL_TOTO_BUFSIZE];
    off_t pos = end;
    off_t out_start = start;
    uintmax_t found = 0;
    int first = 1;
    size_t chunk;
    size_t i;
    ssize_t got;

    while (pos > start) {
        chunk = (pos - start) > (off_t)sizeof buf ? sizeof buf
                                                  : (size_t)(pos - start);
        pos -= (off_t)chunk;
        if (lseek(in_fd, pos, SEEK_SET) < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        got = read_full(in_fd, buf, chunk);
        if (got < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        i = (size_t)got;
        if (first) {
            first = 0;
            if (i > 0 && buf[i - 1] == '\n') {
                i--;
            }
        }
        while (i > 0) {
            i--;
            if (buf[i] == '\n' && ++found == n) {
                out_start = pos + (off_t)i + 1;
                goto emit;
            }
        }
    }

emit:
    if (lseek(in_fd, out_start, SEEK_SET) < 0) {
        return TAIL_TOTO_READ_ERR;
    }
    return copy_to_eof(in_fd, out_fd);
}

/* Seekable, last N bytes (N > 0). */
static enum tail_toto_status seek_bytes_last(int in_fd, int out_fd,
                                             off_t start, off_t end,
                                             uintmax_t n)
{
    off_t pos = start;

    if ((uintmax_t)(end - start) > n) {
        pos = end - (off_t)n;
    }
    if (lseek(in_fd, pos, SEEK_SET) < 0) {
        return TAIL_TOTO_READ_ERR;
    }
    return copy_to_eof(in_fd, out_fd);
}

static void free_chunks(struct line_chunk *head)
{
    struct line_chunk *next;

    while (head != NULL) {
        next = head->next;
        free(head);
        head = next;
    }
}

static uintmax_t count_newlines(const char *p, size_t len)
{
    uintmax_t count = 0;
    const char *hit;

    while (len > 0 && (hit = memchr(p, '\n', len)) != NULL) {
        count++;
        len -= (size_t)(hit - p) + 1;
        p = hit + 1;
    }
    return count;
}

/*
 * Write the last n lines held in the chunk list starting at head.
 * Precondition: the list holds every byte of the output, n > 0.
 */
static enum tail_toto_status emit_chunk_lines(int out_fd,
                                              struct line_chunk *head,
                                              struct line_chunk *tail,
                                              uintmax_t total, uintmax_t n)
{
    struct line_chunk *c = head;
    uintmax_t lines = total;
    uintmax_t skip;
    size_t off = 0;

    if (tail->len > 0 && tail->data[tail->len - 1] != '\n') {
        lines++;
    }
    if (lines > n) {
        skip = lines - n;
        while (c->nlines < skip) {
            skip -= c->nlines;
            c = c->next;
        }
        for (off = 0; off < c->len; off++) {
            if (c->data[off] == '\n' && --skip == 0) {
                off++;
                break;
            }
        }
    }

    for (; c != NULL; c = c->next, off = 0) {
        if (c->len > off
            && tail_toto_write_all(out_fd, c->data + off, c->len - off) != 0) {
            return TAIL_TOTO_WRITE_ERR;
        }
    }
    return TAIL_TOTO_OK;
}

/*
 * Stream, last N lines (N > 0). Reads fill the newest chunk before a new
 * one is allocated. The oldest chunk is dropped while the remaining chunks
 * still contain more than N newlines: the output then starts after a
 * newline that lies entirely in the remaining chunks.
 */
static enum tail_toto_status stream_lines_last(int in_fd, int out_fd,
                                               uintmax_t n)
{
    struct line_chunk *head;
    struct line_chunk *tail;
    struct line_chunk *c;
    enum tail_toto_status status;
    uintmax_t total = 0;
    uintmax_t added;
    ssize_t r;
    int saved;

    head = malloc(sizeof *head);
    if (head == NULL) {
        errno = ENOMEM;
        return TAIL_TOTO_READ_ERR;
    }
    head->next = NULL;
    head->len = 0;
    head->nlines = 0;
    tail = head;

    for (;;) {
        if (tail->len == sizeof tail->data) {
            c = malloc(sizeof *c);
            if (c == NULL) {
                free_chunks(head);
                errno = ENOMEM;
                return TAIL_TOTO_READ_ERR;
            }
            c->next = NULL;
            c->len = 0;
            c->nlines = 0;
            tail->next = c;
            tail = c;
        }

        r = tail_toto_read_retry(in_fd, tail->data + tail->len,
                                 sizeof tail->data - tail->len);
        if (r < 0) {
            saved = errno;
            free_chunks(head);
            errno = saved;
            return TAIL_TOTO_READ_ERR;
        }
        if (r == 0) {
            break;
        }

        added = count_newlines(tail->data + tail->len, (size_t)r);
        tail->len += (size_t)r;
        tail->nlines += added;
        total += added;

        while (head != tail && total - head->nlines > n) {
            total -= head->nlines;
            c = head;
            head = head->next;
            free(c);
        }
    }

    status = emit_chunk_lines(out_fd, head, tail, total, n);
    saved = errno;
    free_chunks(head);
    errno = saved;
    return status;
}

/*
 * Stream, last N bytes (N > 0). The buffer grows (linearly filled, start
 * stays 0) until it reaches N bytes; only then does it wrap as a ring.
 * Growing on demand keeps huge N cheap when the input is small.
 */
static enum tail_toto_status stream_bytes_last(int in_fd, int out_fd,
                                               uintmax_t n)
{
    char tmp[TAIL_TOTO_BUFSIZE];
    size_t limit = n > SIZE_MAX ? SIZE_MAX : (size_t)n;
    enum tail_toto_status status = TAIL_TOTO_OK;
    char *ring = NULL;
    char *grown;
    size_t cap = 0;
    size_t start = 0;
    size_t len = 0;
    size_t got;
    size_t newcap;
    size_t endpos;
    size_t first;
    ssize_t r;
    int saved;

    for (;;) {
        r = tail_toto_read_retry(in_fd, tmp, sizeof tmp);
        if (r < 0) {
            status = TAIL_TOTO_READ_ERR;
            goto done;
        }
        if (r == 0) {
            break;
        }
        got = (size_t)r;

        if (len + got > cap && cap < limit) {
            newcap = cap > 0 ? cap : sizeof tmp;
            while (newcap < len + got && newcap < limit) {
                newcap = newcap > limit / 2 ? limit : newcap * 2;
            }
            if (newcap > limit) {
                newcap = limit;
            }
            grown = realloc(ring, newcap);
            if (grown == NULL) {
                errno = ENOMEM;
                status = TAIL_TOTO_READ_ERR;
                goto done;
            }
            ring = grown;
            cap = newcap;
        }

        if (got >= cap) {
            memcpy(ring, tmp + (got - cap), cap);
            start = 0;
            len = cap;
            continue;
        }
        endpos = (start + len) % cap;
        first = cap - endpos < got ? cap - endpos : got;
        memcpy(ring + endpos, tmp, first);
        memcpy(ring, tmp + first, got - first);
        len += got;
        if (len > cap) {
            start = (start + (len - cap)) % cap;
            len = cap;
        }
    }

    if (len > 0) {
        first = cap - start < len ? cap - start : len;
        if (tail_toto_write_all(out_fd, ring + start, first) != 0
            || tail_toto_write_all(out_fd, ring, len - first) != 0) {
            status = TAIL_TOTO_WRITE_ERR;
        }
    }

done:
    saved = errno;
    free(ring);
    errno = saved;
    return status;
}

/* "+N" lines: skip N - 1 lines (+0 behaves as +1), then copy the rest. */
static enum tail_toto_status lines_from(int in_fd, int out_fd, uintmax_t n)
{
    char buf[TAIL_TOTO_BUFSIZE];
    uintmax_t skip = n > 0 ? n - 1 : 0;
    ssize_t r;
    size_t i;

    while (skip > 0) {
        r = tail_toto_read_retry(in_fd, buf, sizeof buf);
        if (r < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        if (r == 0) {
            return TAIL_TOTO_OK;
        }
        for (i = 0; i < (size_t)r; i++) {
            if (buf[i] == '\n' && --skip == 0) {
                i++;
                if (i < (size_t)r
                    && tail_toto_write_all(out_fd, buf + i,
                                           (size_t)r - i) != 0) {
                    return TAIL_TOTO_WRITE_ERR;
                }
                break;
            }
        }
    }
    return copy_to_eof(in_fd, out_fd);
}

/* "+N" bytes on a non-seekable input: discard N - 1 bytes, copy the rest. */
static enum tail_toto_status stream_bytes_from(int in_fd, int out_fd,
                                               uintmax_t n)
{
    char buf[TAIL_TOTO_BUFSIZE];
    uintmax_t skip = n > 0 ? n - 1 : 0;
    ssize_t r;

    while (skip > 0) {
        r = tail_toto_read_retry(in_fd, buf, sizeof buf);
        if (r < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        if (r == 0) {
            return TAIL_TOTO_OK;
        }
        if ((uintmax_t)r > skip) {
            if (tail_toto_write_all(out_fd, buf + skip,
                                    (size_t)((uintmax_t)r - skip)) != 0) {
                return TAIL_TOTO_WRITE_ERR;
            }
            skip = 0;
        } else {
            skip -= (uintmax_t)r;
        }
    }
    return copy_to_eof(in_fd, out_fd);
}

enum tail_toto_status tail_toto_tail_fd(int in_fd, int out_fd,
                                        const struct tail_toto_opts *opts)
{
    uintmax_t n = opts->count;
    uintmax_t skip;
    off_t start;
    off_t end;

    switch (opts->mode) {
    case TAIL_TOTO_MODE_LINES_LAST:
        if (n == 0) {
            return TAIL_TOTO_OK;
        }
        if (probe_seekable(in_fd, &start, &end)) {
            return seek_lines_last(in_fd, out_fd, start, end, n);
        }
        return stream_lines_last(in_fd, out_fd, n);

    case TAIL_TOTO_MODE_BYTES_LAST:
        if (n == 0) {
            return TAIL_TOTO_OK;
        }
        if (probe_seekable(in_fd, &start, &end)) {
            return seek_bytes_last(in_fd, out_fd, start, end, n);
        }
        return stream_bytes_last(in_fd, out_fd, n);

    case TAIL_TOTO_MODE_LINES_FROM:
        return lines_from(in_fd, out_fd, n);

    case TAIL_TOTO_MODE_BYTES_FROM:
        if (probe_seekable(in_fd, &start, &end)) {
            skip = n > 0 ? n - 1 : 0;
            if ((uintmax_t)(end - start) <= skip) {
                return TAIL_TOTO_OK;
            }
            if (lseek(in_fd, start + (off_t)skip, SEEK_SET) < 0) {
                return TAIL_TOTO_READ_ERR;
            }
            return copy_to_eof(in_fd, out_fd);
        }
        return stream_bytes_from(in_fd, out_fd, n);
    }
    return TAIL_TOTO_OK;
}
