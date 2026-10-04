/*
 * test_runner.c — tail-toto unit test entry point.
 *
 * Chain of thought:
 *   Responsibility: run every scenario in a fixed order. Any assert()
 *                   failure aborts, which makes `make test` fail.
 *   Order:          count_parse -> lines_last -> lines_from -> bytes_last
 *                   -> stream.
 *   Heap:           none.
 *   Standard:       ISO C11.
 */

#include "test_bytes_last.h"
#include "test_count_parse.h"
#include "test_lines_from.h"
#include "test_lines_last.h"
#include "test_stream.h"

int main(void)
{
    test_count_parse_run();
    test_lines_last_run();
    test_lines_from_run();
    test_bytes_last_run();
    test_stream_run();
    return 0;
}
