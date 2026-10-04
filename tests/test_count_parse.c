/*
 * test_count_parse.c — tail_toto_parse_count() accepts [+|-]DIGITS only.
 *
 * Chain of thought:
 *   Responsibility: valid forms yield the right value and '+' flag; huge
 *                   values saturate to UINTMAX_MAX; everything else is
 *                   rejected with -1 (no diagnostics are printed).
 *   Heap:           none.
 *   Standard:       ISO C11.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "tail_toto_cli.h"
#include "test_count_parse.h"

static void expect_valid(const char *s, uintmax_t value, int from_start)
{
    uintmax_t got = 0;
    int from = -1;
    int rc = tail_toto_parse_count(s, &got, &from);

    assert(rc == 0);
    assert(got == value);
    assert(from == from_start);
    (void)rc;
    (void)value;
    (void)from_start;
}

static void expect_invalid(const char *s)
{
    uintmax_t got = 0;
    int from = 0;
    int rc = tail_toto_parse_count(s, &got, &from);

    assert(rc == -1);
    (void)rc;
}

void test_count_parse_run(void)
{
    expect_valid("10", 10, 0);
    expect_valid("0", 0, 0);
    expect_valid("+5", 5, 1);
    expect_valid("+0", 0, 1);
    expect_valid("-7", 7, 0);
    expect_valid("007", 7, 0);
    printf("PASS: count_parse valid forms\n");

    expect_valid("99999999999999999999999999999999", UINTMAX_MAX, 0);
    expect_valid("+99999999999999999999999999999999", UINTMAX_MAX, 1);
    printf("PASS: count_parse overflow saturates\n");

    expect_invalid("");
    expect_invalid("+");
    expect_invalid("-");
    expect_invalid("abc");
    expect_invalid("5x");
    expect_invalid(" 5");
    expect_invalid("5 ");
    expect_invalid("--5");
    expect_invalid("+-5");
    expect_invalid("1.5");
    expect_invalid("5K");
    printf("PASS: count_parse invalid forms rejected\n");
}
