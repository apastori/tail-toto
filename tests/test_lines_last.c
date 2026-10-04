/*
 * test_lines_last.c — "-n N" on seekable regular files.
 *
 * Chain of thought:
 *   Responsibility: exercise the backwards scan: default count, N larger
 *                   than the file, N = 0, unterminated last line, empty
 *                   file, a newline exactly on either side of a
 *                   TAIL_TOTO_BUFSIZE chunk boundary, a line longer than a
 *                   chunk, and a non-zero starting offset.
 *   Heap:           none in the test; stack buffers only.
 *   Standard:       ISO C11 + POSIX.1-2008.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tail_toto_io.h"
#include "test_lines_last.h"
#include "test_util.h"

#define LINES TAIL_TOTO_MODE_LINES_LAST

static void expect_lines(const char *in, size_t in_len, uintmax_t n,
                         const char *want, size_t want_len)
{
    char out[TEST_UTIL_OUT_MAX];
    size_t got = test_util_tail_buffer(in, in_len, LINES, n, out, sizeof out);

    test_util_expect(out, got, want, want_len);
}

#define EXPECT_STR(in, n, want) \
    expect_lines((in), strlen(in), (n), (want), strlen(want))

/*
 * Build 'a' * 100 + '\n' + 'b' * blen + '\n' and check that -n 1 returns
 * only the b-line. Choosing blen moves the first '\n' relative to the
 * chunk boundary at end - TAIL_TOTO_BUFSIZE.
 */
static void expect_boundary(size_t blen)
{
    char in[TEST_UTIL_IN_MAX];
    size_t len = 0;

    memset(in, 'a', 100);
    len += 100;
    in[len++] = '\n';
    memset(in + len, 'b', blen);
    len += blen;
    in[len++] = '\n';
    expect_lines(in, len, 1, in + 101, blen + 1);
}

static void check_nonzero_offset(void)
{
    static const char data[] = "a\nb\nc\n";
    char out[TEST_UTIL_OUT_MAX];
    FILE *fp;
    int fd = test_util_file_from(data, sizeof data - 1, &fp);
    off_t pos = lseek(fd, 2, SEEK_SET);
    size_t got;

    assert(pos == 2);
    (void)pos;
    got = test_util_capture(fd, LINES, 10, out, sizeof out);
    test_util_expect(out, got, "b\nc\n", 4);
    fclose(fp);
}

void test_lines_last_run(void)
{
    char in[TEST_UTIL_IN_MAX];
    char want[TEST_UTIL_IN_MAX];
    size_t in_len;
    size_t want_len;

    EXPECT_STR("a\nb\nc\n", 2, "b\nc\n");
    printf("PASS: lines_last basic\n");

    in_len = test_util_numbered_lines(in, sizeof in, 1, 15);
    want_len = test_util_numbered_lines(want, sizeof want, 6, 15);
    expect_lines(in, in_len, 10, want, want_len);
    printf("PASS: lines_last default count of 10\n");

    EXPECT_STR("a\nb\nc\n", 100, "a\nb\nc\n");
    printf("PASS: lines_last count larger than file\n");

    EXPECT_STR("a\nb\nc\n", 0, "");
    printf("PASS: lines_last zero count\n");

    EXPECT_STR("a\nb", 1, "b");
    EXPECT_STR("a\nb", 2, "a\nb");
    printf("PASS: lines_last unterminated last line\n");

    EXPECT_STR("", 10, "");
    printf("PASS: lines_last empty file\n");

    expect_boundary(TAIL_TOTO_BUFSIZE - 2);
    expect_boundary(TAIL_TOTO_BUFSIZE - 1);
    printf("PASS: lines_last newline on chunk boundary\n");

    expect_boundary(TAIL_TOTO_BUFSIZE + 500);
    printf("PASS: lines_last line longer than a chunk\n");

    in_len = test_util_numbered_lines(in, sizeof in, 1, 3000);
    want_len = test_util_numbered_lines(want, sizeof want, 2998, 3000);
    expect_lines(in, in_len, 3, want, want_len);
    printf("PASS: lines_last multi-chunk file\n");

    check_nonzero_offset();
    printf("PASS: lines_last starts at current offset\n");
}
