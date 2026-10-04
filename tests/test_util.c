/*
 * test_util.c — shared helpers for tail-toto unit tests.
 *
 * Chain of thought:
 *   Responsibility: build input files and capture the output of
 *                   tail_toto_tail_fd() so scenarios compare plain buffers.
 *   Files:          tmpfile() is ISO C and works on POSIX and UCRT; only the
 *                   underlying fd is used, so stdio buffering never mixes
 *                   with raw read()/write().
 *   Heap:           none in test code; caller-provided stack buffers only.
 *   Standard:       ISO C11 + POSIX.1-2008.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tail_toto_io.h"
#include "tail_toto_tail.h"
#include "test_util.h"

/* MinGW hides the POSIX fileno() name under -std=c11. */
#ifdef TAIL_TOTO_WIN32_IO
#define TEST_FILENO _fileno
#else
#define TEST_FILENO fileno
#endif

int test_util_temp_fd(FILE **fp)
{
    int fd;

    *fp = tmpfile();
    assert(*fp != NULL);
    fd = TEST_FILENO(*fp);
    assert(fd >= 0);
    return fd;
}

int test_util_file_from(const char *data, size_t len, FILE **fp)
{
    int fd = test_util_temp_fd(fp);
    int rc = tail_toto_write_all(fd, data, len);
    off_t pos;

    assert(rc == 0);
    pos = lseek(fd, 0, SEEK_SET);
    assert(pos == 0);
    (void)rc;
    (void)pos;
    return fd;
}

size_t test_util_capture(int in_fd, enum tail_toto_mode mode,
                         uintmax_t count, char *out, size_t cap)
{
    struct tail_toto_opts opts;
    enum tail_toto_status status;
    FILE *fp;
    int out_fd = test_util_temp_fd(&fp);
    size_t total = 0;
    ssize_t n;
    off_t pos;

    opts.mode = mode;
    opts.count = count;
    opts.headers = -1;
    opts.nfiles = 0;
    opts.files = NULL;

    status = tail_toto_tail_fd(in_fd, out_fd, &opts);
    assert(status == TAIL_TOTO_OK);
    (void)status;

    pos = lseek(out_fd, 0, SEEK_SET);
    assert(pos == 0);
    (void)pos;

    while ((n = tail_toto_read_retry(out_fd, out + total, cap - total)) > 0) {
        total += (size_t)n;
        assert(total < cap);
    }
    assert(n == 0);

    fclose(fp);
    return total;
}

size_t test_util_tail_buffer(const char *data, size_t len,
                             enum tail_toto_mode mode, uintmax_t count,
                             char *out, size_t cap)
{
    FILE *fp;
    int fd = test_util_file_from(data, len, &fp);
    size_t got = test_util_capture(fd, mode, count, out, cap);

    fclose(fp);
    return got;
}

void test_util_expect(const char *got, size_t got_len,
                      const char *want, size_t want_len)
{
    assert(got_len == want_len);
    assert(want_len == 0 || memcmp(got, want, want_len) == 0);
    (void)got;
    (void)got_len;
    (void)want;
    (void)want_len;
}

size_t test_util_numbered_lines(char *buf, size_t cap,
                                unsigned first, unsigned last)
{
    size_t len = 0;
    unsigned i;
    int n;

    for (i = first; i <= last; i++) {
        n = snprintf(buf + len, cap - len, "%u\n", i);
        assert(n > 0 && (size_t)n < cap - len);
        len += (size_t)n;
    }
    return len;
}
