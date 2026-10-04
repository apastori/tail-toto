/*
 * test_bytes_last.c — "-c N" and "-c +N" on seekable regular files.
 *
 * Chain of thought:
 *   Responsibility: N smaller than, equal to, and larger than the file; N = 0;
 *                   +N from the start including +0, +1 and past EOF; a
 *                   multi-chunk file in both modes.
 *   Heap:           none.
 *   Standard:       ISO C11 + POSIX.1-2008.
 */

#include <stdio.h>
#include <string.h>

#include "test_bytes_last.h"
#include "test_util.h"

static void expect_bytes(const char *in, size_t in_len,
                         enum tail_toto_mode mode, uintmax_t n,
                         const char *want, size_t want_len)
{
    char out[TEST_UTIL_OUT_MAX];
    size_t got = test_util_tail_buffer(in, in_len, mode, n, out, sizeof out);

    test_util_expect(out, got, want, want_len);
}

#define EXPECT_LAST(in, n, want) \
    expect_bytes((in), strlen(in), TAIL_TOTO_MODE_BYTES_LAST, (n), \
                 (want), strlen(want))
#define EXPECT_FROM(in, n, want) \
    expect_bytes((in), strlen(in), TAIL_TOTO_MODE_BYTES_FROM, (n), \
                 (want), strlen(want))

void test_bytes_last_run(void)
{
    char in[TEST_UTIL_IN_MAX];
    size_t in_len;

    EXPECT_LAST("abcdef", 2, "ef");
    EXPECT_LAST("abcdef", 6, "abcdef");
    EXPECT_LAST("abcdef", 100, "abcdef");
    printf("PASS: bytes_last less, equal, greater than size\n");

    EXPECT_LAST("abcdef", 0, "");
    EXPECT_LAST("", 5, "");
    printf("PASS: bytes_last zero count and empty file\n");

    EXPECT_FROM("abcdef", 1, "abcdef");
    EXPECT_FROM("abcdef", 0, "abcdef");
    EXPECT_FROM("abcdef", 3, "cdef");
    EXPECT_FROM("abcdef", 6, "f");
    EXPECT_FROM("abcdef", 7, "");
    EXPECT_FROM("abcdef", 100, "");
    printf("PASS: bytes_from +N\n");

    in_len = test_util_numbered_lines(in, sizeof in, 1, 3000);
    expect_bytes(in, in_len, TAIL_TOTO_MODE_BYTES_LAST, 10000,
                 in + in_len - 10000, 10000);
    expect_bytes(in, in_len, TAIL_TOTO_MODE_BYTES_FROM, in_len - 5,
                 in + in_len - 6, 6);
    printf("PASS: bytes multi-chunk file\n");
}
