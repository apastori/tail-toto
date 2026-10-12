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
    /* check if the file is a regular file */
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }
    /* get the current offset */
    *start = lseek(fd, 0, SEEK_CUR);
    if (*start < 0) {
        return 0;
    }
    /* get the file size */
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

    /* read the file until the desired length is reached or EOF is reached */
    while (got < len) {
        n = tail_toto_read_retry(fd, buf + got, len - got);
        if (n < 0) {
            /* if the read fails, return -1 */
            return -1;
        }
        if (n == 0) {
            /* the file has been completely read */
            break;
        }
        got += (size_t)n;
    }
    /* return the number of bytes read */
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
    int n_lines_found = 0;

    /* scan backwards in TAIL_TOTO_BUFSIZE chunks counting '\n' until the Nth one before the final line is found */
    while (pos > start) {
        /* get the chunk size */
        chunk = (pos - start) > (off_t)sizeof buf ? sizeof buf
                                                  : (size_t)(pos - start);
        /* Move pointer from current position that started at end of file to the start of the chunk */
        pos -= (off_t)chunk;
        /* move the file pointer to the start of the chunk */
        if (lseek(in_fd, pos, SEEK_SET) < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        /* read the chunk */
        got = read_full(in_fd, buf, chunk);
        if (got < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        i = (size_t)got;
        /* if it's the first chunk, check if the last character is a '\n' */
        if (first) {
            /* if it's the first chunk, skip the last character if it a end of line */
            first = 0;
            /* if the last character is a '\n', skip it */
            if (i > 0 && buf[i - 1] == '\n') {
                i--;
            }
        }
        /* count the number of '\n' in the chunk backwards, i starts at end of */
        /* buffer chunk and decrements until 0, found is the number of '\n' found */
        while (i > 0) {
            i--;
            if (buf[i] == '\n') {
                found++;
                /* if the Nth '\n' is found, set the output start position and break the loop*/
                if (found == n) {
                    out_start = pos + (off_t)i + 1;
                    n_lines_found = 1;
                    break;
                }
            }
        }
        /* if the Nth '\n' is found, break out of the loop */
        if (n_lines_found) {
            break;
        }
    }
    /* change the file pointer to the output start position */
    if (lseek(in_fd, out_start, SEEK_SET) < 0) {
        return TAIL_TOTO_READ_ERR;
    }
    /* copy the lines from the output start position to the end of the file */
    return copy_to_eof(in_fd, out_fd);
}

/* Seekable, last N bytes (N > 0). */
static enum tail_toto_status seek_bytes_last(int in_fd, int out_fd,
                                             off_t start, off_t end,
                                             uintmax_t n)
{
    off_t pos = start;
    /* if the file is larger than N bytes, set the position to the end of the file minus N bytes */
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

    while (len > 0) {
        /* find the next newline in the buffer */
        hit = memchr(p, '\n', len);
        if (hit == NULL) {
            break;
        }
        count++;
        /* update the length of the buffer */
        len -= (size_t)(hit - p) + 1;
        /* update the pointer to the next character after the newline */
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
                                              char last,
                                              uintmax_t total, uintmax_t n)
{
    struct line_chunk *c = head;
    uintmax_t lines = total;
    uintmax_t skip;
    size_t off = 0;

    /* an unterminated final line still counts as a line */
    if (last != '\n') {
        lines++;
    }
    /* if the number of lines is greater than N, skip until the Nth line */
    if (lines > n) {
        /* lines to skip */
        skip = lines - n;
        /* cut linked list until the Nth line */
        while (c->nlines < skip) {
            /* update the number of lines to skip */
            skip -= c->nlines;
            /* delete first chunk head */
            c = c->next;
        }
        /* find the first newline in the chunk */
        for (off = 0; off < c->len; off++) {
            /* if the character is not a newline, continue */
            if (c->data[off] != '\n') {
                continue;
            }
            /* update the number of lines to skip */
            skip--;
            /* if the number of lines to skip is 0, break out of the loop */
            if (skip == 0) {
                /* update the offset to the first character after the newline */
                /* the target output offset */
                off++;
                break;
            }
        }
    }

    /* write the lines to the output file */
    while (c != NULL) {
        /* skip the chunk if the offset is greater than the length of the chunk */
        if (c->len > off) {
            /* write the chunk to the output file */
            if (tail_toto_write_all(out_fd, c->data + off, c->len - off) != 0) {
                return TAIL_TOTO_WRITE_ERR;
            }
        }
        /* update the chunk pointer to the next chunk */
        c = c->next;
        /* reset the offset to 0 */
        off = 0;
    }
    return TAIL_TOTO_OK;
}

static void init_chunk(struct line_chunk *c)
{
    c->next = NULL;
    c->len = 0;
    c->nlines = 0;
}

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
    char last = '\n';

    // initialize the head of the chunk linked list
    head = malloc(sizeof *head);
    /* if the allocation fails, return an error */
    if (head == NULL) {
        errno = ENOMEM;
        return TAIL_TOTO_READ_ERR;
    }
    /* initialize the head of the chunk linked list */
    init_chunk(head);
    /* set the tail to the head since there is only one chunk initially */
    tail = head;
    /* read the input until the end */
    for (;;) {
        /* if the current chunk is full, allocate a new chunk */
        if (tail->len == sizeof tail->data) {
            /* allocate a new chunk */
            c = malloc(sizeof *c);
            if (c == NULL) {
                /* if the allocation fails, free the chunk linked list and return an error */
                free_chunks(head);
                errno = ENOMEM;
                return TAIL_TOTO_READ_ERR;
            }
            /* initialize the new chunk */
            init_chunk(c);
            /* set the next pointer of the current tail to the new chunk */
            tail->next = c;
            /* update the tail pointer to the new chunk */
            tail = c;
        }
        /* Read data into the current chunk */
        r = tail_toto_read_retry(in_fd, tail->data + tail->len,
                                 sizeof tail->data - tail->len);
        if (r < 0) {
            /* save the error number before freeing the chunk linked list */
            saved = errno;
            /* free the chunk linked list and return an error */
            free_chunks(head);
            errno = saved;
            return TAIL_TOTO_READ_ERR;
        }

        /* if the end of the input is reached, break out of the loop */
        if (r == 0) {
            break;
        }
        /* count the number of newlines in the current chunk */
        added = count_newlines(tail->data + tail->len, (size_t)r);
        /* update the length of the current chunk */
        tail->len += (size_t)r;
        last = tail->data[tail->len - 1];
        /* update the number of newlines in the current chunk */
        tail->nlines += added;
        /* update the total number of newlines */
        total += added;

        /* if the total number of newlines is greater than N, free the oldest chunk */
        /* unless there is only one chunk in the linked list */
        while (head != tail && total - head->nlines > n) {
            /* update the total number of newlines */
            total -= head->nlines;
            /* save head memory address before updating the head pointer */
            c = head;
            /* update the head pointer to the next chunk */
            head = head->next;
            free(c);
        }
    }

    status = emit_chunk_lines(out_fd, head, last, total, n);
    /* save the error number before freeing the chunk linked list */
    saved = errno;
    /* free the chunk linked list and return the status */
    free_chunks(head);
    /* restore the error number */
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
        // if the read fails, set the status to an error and break out of the loop
        if (r < 0) {
            status = TAIL_TOTO_READ_ERR;
            break;
        }
        // if the end of the input is reached, break out of the loop
        if (r == 0) {
            break;
        }
        got = (size_t)r;

        // if the length of the buffer is greater than the capacity and the capacity is less than the limit
        if (len + got > cap && cap < limit) {
            // if the capacity is not greater than 0, set the capacity to the current buffer temp */
            newcap = cap > 0 ? cap : sizeof tmp;
            // while the new capacity is less than the length of the buffer plus the number of bytes read 
            // and the new capacity is less than the limit
            // double the capacity until it is greater than the limit
            while (newcap < len + got && newcap < limit) {
                newcap = newcap > limit / 2 ? limit : newcap * 2;
            }
            // if the new capacity is greater than the limit
            // set the new capacity to the limit to avoid overflow
            if (newcap > limit) {
                newcap = limit;
            }
            grown = realloc(ring, newcap);
            // if the reallocation fails, set the status to an error and break out of the loop
            if (grown == NULL) {
                errno = ENOMEM;
                status = TAIL_TOTO_READ_ERR;
                break;
            }
            ring = grown;
            cap = newcap;
        }
        /* if the number of bytes read is greater than or equal to the capacity */
        /* everything in the ring is older than the new data */
        if (got >= cap) {
            memcpy(ring, tmp + (got - cap), cap);
            start = 0;
            len = cap;
            continue;
        }
        /* calculate the position of the next write */
        endpos = (start + len) % cap;
        /* the number of bytes to copy from the temporary buffer to the ring */
        first = cap - endpos < got ? cap - endpos : got;
        /* copy the new data to the tail of the ring */
        memcpy(ring + endpos, tmp, first);
        /* copy the remaining data to the head of the ring */
        memcpy(ring, tmp + first, got - first);
        len += got;
        /* if the length of the ring is greater than the capacity */
        /* the ring is full, so wrap around to the beginning */
        if (len > cap) {
            start = (start + (len - cap)) % cap;
            len = cap;
        }
    }

    if (status == TAIL_TOTO_OK && len > 0) {
        first = cap - start < len ? cap - start : len;
        if (tail_toto_write_all(out_fd, ring + start, first) != 0
            || tail_toto_write_all(out_fd, ring, len - first) != 0) {
            status = TAIL_TOTO_WRITE_ERR;
        }
    }

    /* free the ring buffer and return the status */
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
        /* if the read fails, return an error */
        if (r < 0) {
            return TAIL_TOTO_READ_ERR;
        }
        /* if the end of the input is reached, return OK */
        if (r == 0) {
            return TAIL_TOTO_OK;
        }
        /* iterate through the buffer */
        for (i = 0; i < (size_t)r; i++) {
            // if the character is not a newline, continue
            if (buf[i] != '\n') {
                continue;
            }
            /* if the character is a newline and the number of lines to skip is 0 */
            skip--;
            if (skip == 0) {
                /* write the rest of the buffer to the output file */
                i++;
                if (i < (size_t)r && tail_toto_write_all(out_fd, buf + i, (size_t)r - i) != 0) {
                    return TAIL_TOTO_WRITE_ERR; /* return an error */
                }
                break; /* break out of the loop */
            }
        }
    }
    // copy after skipping the lines
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

    /* handle the last N lines mode */
    if (opts->mode == TAIL_TOTO_MODE_LINES_LAST) {
        if (n == 0) {
            return TAIL_TOTO_OK;
        }
        if (probe_seekable(in_fd, &start, &end)) {
            /* seek to the last N lines */
            return seek_lines_last(in_fd, out_fd, start, end, n);
        }
        /* stream the last N lines */
        return stream_lines_last(in_fd, out_fd, n);
    }

    /* handle the last N bytes mode */
    if (opts->mode == TAIL_TOTO_MODE_BYTES_LAST) {
        if (n == 0) {
            return TAIL_TOTO_OK;
        }
        if (probe_seekable(in_fd, &start, &end)) {
            /* seek to the last N bytes */
            return seek_bytes_last(in_fd, out_fd, start, end, n);
        }
        /* stream the last N bytes */
        return stream_bytes_last(in_fd, out_fd, n);
    }

    /* handle the +N lines mode */
    if (opts->mode == TAIL_TOTO_MODE_LINES_FROM) {
        return lines_from(in_fd, out_fd, n);
    }

    /* handle the +N bytes mode */
    if (opts->mode == TAIL_TOTO_MODE_BYTES_FROM) {
        if (probe_seekable(in_fd, &start, &end)) {
            skip = n > 0 ? n - 1 : 0;
            /* check if the file is large enough to skip N - 1 lines */
            if ((uintmax_t)(end - start) <= skip) {
                /* the file is not large enough to skip N - 1 lines, 
                /* return OK nothing to print out*/
                return TAIL_TOTO_OK;
            }
            /* seek to the offset after skipping N - 1 lines */
            if (lseek(in_fd, start + (off_t)skip, SEEK_SET) < 0) {
                return TAIL_TOTO_READ_ERR;
            }
            /* copy the rest of the file */
            return copy_to_eof(in_fd, out_fd);
        }
        /* stream the bytes after skipping N - 1 bytes */
        return stream_bytes_from(in_fd, out_fd, n);
    }

    /* default to OK */
    return TAIL_TOTO_OK;
}
