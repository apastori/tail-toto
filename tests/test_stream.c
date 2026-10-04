/*
 * test_stream.c — tail algorithms on non-seekable (pipe) input.
 *
 * Chain of thought:
 *   Responsibility: drive the stream path (chunk list for lines, ring buffer
 *                   for bytes, forward skip for +N) through a pipe(2), with
 *                   the same expectations as the seekable scenarios.
 *   Platform:       pipe() is POSIX; Win32 I/O builds print SKIP.
 *   Pipe capacity:  inputs stay below 64 KiB so the whole input can be
 *                   written before the read end is drained.
 *   Heap:           none in the test (the code under test uses the heap).
 *   Standard:       ISO C11 + POSIX.1-2008.
 */

#include <stdio.h>

#include "test_stream.h"

#ifndef TAIL_TOTO_WIN32_IO

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "tail_toto_io.h"
#include "test_util.h"

static void expect_pipe(const char *in, size_t in_len,
                        enum tail_toto_mode mode, uintmax_t n,
                        const char *want, size_t want_len)
{
    char out[TEST_UTIL_OUT_MAX];
    int fds[2];
    int rc = pipe(fds);
    size_t got;

    assert(rc == 0);
    rc = tail_toto_write_all(fds[1], in, in_len);
    assert(rc == 0);
    (void)rc;
    close(fds[1]);

    got = test_util_capture(fds[0], mode, n, out, sizeof out);
    close(fds[0]);
    test_util_expect(out, got, want, want_len);
}

#define EXPECT_STR(in, mode, n, want) \
    expect_pipe((in), strlen(in), (mode), (n), (want), strlen(want))

void test_stream_run(void)
{
    char in[TEST_UTIL_IN_MAX];
    char want[TEST_UTIL_IN_MAX];
    size_t in_len;
    size_t want_len;

    EXPECT_STR("a\nb\nc\n", TAIL_TOTO_MODE_LINES_LAST, 2, "b\nc\n");
    EXPECT_STR("a\nb\nc\n", TAIL_TOTO_MODE_LINES_LAST, 100, "a\nb\nc\n");
    EXPECT_STR("a\nb\nc\n", TAIL_TOTO_MODE_LINES_LAST, 0, "");
    EXPECT_STR("a\nb", TAIL_TOTO_MODE_LINES_LAST, 1, "b");
    EXPECT_STR("", TAIL_TOTO_MODE_LINES_LAST, 10, "");
    printf("PASS: stream lines_last basic\n");

    in_len = test_util_numbered_lines(in, sizeof in, 1, 3000);
    want_len = test_util_numbered_lines(want, sizeof want, 2998, 3000);
    expect_pipe(in, in_len, TAIL_TOTO_MODE_LINES_LAST, 3, want, want_len);
    expect_pipe(in, in_len, TAIL_TOTO_MODE_LINES_LAST, 5000, in, in_len);
    want_len = test_util_numbered_lines(want, sizeof want, 1001, 3000);
    expect_pipe(in, in_len, TAIL_TOTO_MODE_LINES_LAST, 2000, want, want_len);
    printf("PASS: stream lines_last multi-chunk with eviction\n");

    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_LAST, 2, "ef");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_LAST, 6, "abcdef");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_LAST, 100, "abcdef");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_LAST, 0, "");
    printf("PASS: stream bytes_last basic\n");

    expect_pipe(in, in_len, TAIL_TOTO_MODE_BYTES_LAST, 10,
                in + in_len - 10, 10);
    expect_pipe(in, in_len, TAIL_TOTO_MODE_BYTES_LAST, 9000,
                in + in_len - 9000, 9000);
    printf("PASS: stream bytes_last ring buffer wrap and growth\n");

    EXPECT_STR("a\nb\nc\n", TAIL_TOTO_MODE_LINES_FROM, 2, "b\nc\n");
    EXPECT_STR("a\nb\nc\n", TAIL_TOTO_MODE_LINES_FROM, 10, "");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_FROM, 3, "cdef");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_FROM, 0, "abcdef");
    EXPECT_STR("abcdef", TAIL_TOTO_MODE_BYTES_FROM, 7, "");
    expect_pipe(in, in_len, TAIL_TOTO_MODE_BYTES_FROM, in_len - 9999,
                in + in_len - 10000, 10000);
    printf("PASS: stream +N lines and bytes\n");
}

#else /* TAIL_TOTO_WIN32_IO */

void test_stream_run(void)
{
    printf("SKIP: stream (pipe-based tests are POSIX only)\n");
}

#endif /* TAIL_TOTO_WIN32_IO */
