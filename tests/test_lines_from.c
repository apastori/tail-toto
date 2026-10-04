/*
 * test_lines_from.c — "-n +N" on seekable regular files.
 *
 * Chain of thought:
 *   Responsibility: +1 and +0 copy the whole file, +N skips N - 1 lines,
 *                   +N past EOF produces nothing, and the skip works across
 *                   TAIL_TOTO_BUFSIZE chunks.
 *   Heap:           none.
 *   Standard:       ISO C11 + POSIX.1-2008.
 */

#include <stdio.h>
#include <string.h>

#include "test_lines_from.h"
#include "test_util.h"

#define FROM TAIL_TOTO_MODE_LINES_FROM

static void expect_from(const char *in, size_t in_len, uintmax_t n,
                        const char *want, size_t want_len)
{
    char out[TEST_UTIL_OUT_MAX];
    size_t got = test_util_tail_buffer(in, in_len, FROM, n, out, sizeof out);

    test_util_expect(out, got, want, want_len);
}

#define EXPECT_STR(in, n, want) \
    expect_from((in), strlen(in), (n), (want), strlen(want))

void test_lines_from_run(void)
{
    char in[TEST_UTIL_IN_MAX];
    char want[TEST_UTIL_IN_MAX];
    size_t in_len;
    size_t want_len;

    EXPECT_STR("a\nb\nc\n", 1, "a\nb\nc\n");
    EXPECT_STR("a\nb\nc\n", 0, "a\nb\nc\n");
    printf("PASS: lines_from +1 and +0 copy everything\n");

    EXPECT_STR("a\nb\nc\n", 2, "b\nc\n");
    EXPECT_STR("a\nb\nc\n", 3, "c\n");
    EXPECT_STR("a\nb", 2, "b");
    printf("PASS: lines_from skips leading lines\n");

    EXPECT_STR("a\nb\nc\n", 4, "");
    EXPECT_STR("a\nb\nc\n", 10, "");
    EXPECT_STR("", 1, "");
    printf("PASS: lines_from past end of file\n");

    in_len = test_util_numbered_lines(in, sizeof in, 1, 3000);
    want_len = test_util_numbered_lines(want, sizeof want, 2999, 3000);
    expect_from(in, in_len, 2999, want, want_len);
    printf("PASS: lines_from multi-chunk file\n");
}
