/*
 * test_util.h — shared helpers for tail-toto unit tests (not a scenario).
 */

#ifndef TAIL_TOTO_TEST_UTIL_H
#define TAIL_TOTO_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "tail_toto.h"

#define TEST_UTIL_IN_MAX 20000
#define TEST_UTIL_OUT_MAX 24000

/* Create an anonymous temporary file; returns its fd, *fp owns it. */
int test_util_temp_fd(FILE **fp);

/* Temporary file holding data[0..len), fd positioned at offset 0. */
int test_util_file_from(const char *data, size_t len, FILE **fp);

/*
 * Run tail_toto_tail_fd(in_fd, <temp file>, mode/count), assert it returns
 * TAIL_TOTO_OK, and copy the produced bytes into out.
 * Returns: number of bytes produced (asserted < cap).
 */
size_t test_util_capture(int in_fd, enum tail_toto_mode mode,
                         uintmax_t count, char *out, size_t cap);

/* test_util_file_from() + test_util_capture() on a regular (seekable) file. */
size_t test_util_tail_buffer(const char *data, size_t len,
                             enum tail_toto_mode mode, uintmax_t count,
                             char *out, size_t cap);

/* Assert got[0..got_len) equals want[0..want_len). */
void test_util_expect(const char *got, size_t got_len,
                      const char *want, size_t want_len);

/* Fill buf with "first\n" .. "last\n"; returns the byte length. */
size_t test_util_numbered_lines(char *buf, size_t cap,
                                unsigned first, unsigned last);

#endif /* TAIL_TOTO_TEST_UTIL_H */
