/*
 * tail_toto_cli.c — argument parsing, count parsing, help and version text.
 *
 * Chain of thought:
 *   Responsibility: turn argv into struct tail_toto_opts. Flag spellings come
 *                   only from the TAIL_TOTO_ARG_* macros in tail_toto_cli.h.
 *   Parsing:        hand-rolled (getopt_long is a GNU extension). Supports
 *                   bundled short flags, -nNUM / -n NUM, --lines=NUM /
 *                   --lines NUM, "--" to end options, and "-" as an operand.
 *                   Options and operands may be interleaved, as in GNU tail.
 *   Syscalls:       none; help/version use stdio on stdout, which is allowed
 *                   because no raw write() output happens on that path.
 *   Heap:           none. Operands are compacted in place inside argv, which
 *                   C11 (5.1.2.2.1) guarantees is modifiable.
 *   Standard:       ISO C11 (strtoumax from <inttypes.h>).
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "tail_toto.h"
#include "tail_toto_cli.h"
#include "tail_toto_emit.h"

#define SHORT_CHAR(flag) ((flag)[1])

static char stdin_operand[] = TAIL_TOTO_ARG_STDIN;
static char *default_files[] = { stdin_operand };

/* Returns 1 when argv[1..] (up to "--") contains either spelling exactly. */
static int argv_has_exact(int argc, char **argv, const char *needle)
{
    int i;

    for (i = 1; i < argc; i++) {
        const char *haystack = argv[i];
        if (strcmp(haystack, needle) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Index of the first "--" in argv[1..], or argc when there is none. */
static int meta_scan_limit(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], TAIL_TOTO_ARG_END) == 0) {
            return i;
        }
    }
    return argc;
}

tail_toto_meta_t scan_meta_flags(int argc, char **argv)
{
    int limit;
    limit = meta_scan_limit(argc, argv);
    if (argv_has_exact(limit, argv, TAIL_TOTO_ARG_HELP) || 
        argv_has_exact(limit, argv, TAIL_TOTO_ARG_HELP_SHORT)) {
        return TAIL_TOTO_META_HELP;
    }
    if (argv_has_exact(limit, argv, TAIL_TOTO_ARG_VERSION) ||
        argv_has_exact(limit, argv, TAIL_TOTO_ARG_VERSION_SHORT)) {
        return TAIL_TOTO_META_VERSION;
    }
    return TAIL_TOTO_META_NONE;
}

int tail_toto_parse_count(const char *s, uintmax_t *out, int *from_start)
{
    const char *p;
    uintmax_t value;

    *from_start = 0;
    if (*s == '+') {
        *from_start = 1;
        s++;
    } else if (*s == '-') {
        s++;
    }

    if (*s == '\0') {
        return -1;
    }
    for (p = s; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            return -1;
        }
    }

    errno = 0;
    value = strtoumax(s, NULL, 10);
    if (errno == ERANGE) {
        value = UINTMAX_MAX;
    }
    *out = value;
    return 0;
}

/*
 * Parse value as a -n / -c count and store it in opts.
 * Returns 0, or -1 after emitting "invalid number of ..." to stderr.
 */
static int apply_count(struct tail_toto_opts *opts, int bytes,
                       const char *value)
{
    uintmax_t count;
    int from_start;

    if (tail_toto_parse_count(value, &count, &from_start) != 0) {
        tail_toto_emit_invalid_count(bytes, value);
        return -1;
    }
    opts->count = count;
    if (bytes) {
        opts->mode = from_start ? TAIL_TOTO_MODE_BYTES_FROM
                                : TAIL_TOTO_MODE_BYTES_LAST;
    } else {
        opts->mode = from_start ? TAIL_TOTO_MODE_LINES_FROM
                                : TAIL_TOTO_MODE_LINES_LAST;
    }
    return 0;
}

/*
 * If arg is "<name>" or "<name>=VALUE", return 1 and set *value to the text
 * after '=' (or NULL when the value must come from the next argument).
 * Returns 0 when arg does not name this option.
 */
static int match_long_value(const char *arg, const char *name,
                            const char **value)
{
    size_t len = strlen(name);

    if (strncmp(arg, name, len) != 0) {
        return 0;
    }
    if (arg[len] == '\0') {
        *value = NULL;
        return 1;
    }
    if (arg[len] == '=') {
        *value = arg + len + 1;
        return 1;
    }
    return 0;
}

/*
 * Handle one "--name[=value]" argument at argv[*i]; may consume argv[*i + 1].
 * Returns 0, or -1 after emitting a diagnostic.
 */
static int parse_long(int argc, char **argv, int *i,
                      struct tail_toto_opts *opts)
{
    const char *arg = argv[*i];
    const char *value;
    int bytes;

    /* Handle quiet long option --quiet or silent long option --silent */
    if (strcmp(arg, TAIL_TOTO_ARG_QUIET) == 0
        || strcmp(arg, TAIL_TOTO_ARG_SILENT) == 0) {
        opts->headers = 0;
        return 0;
    }
    
    /* Handle verbose long option --verbose */
    if (strcmp(arg, TAIL_TOTO_ARG_VERBOSE) == 0) {
        opts->headers = 1;
        return 0;
    }

    if (match_long_value(arg, TAIL_TOTO_ARG_LINES, &value)) {
        bytes = 0;
    } else if (match_long_value(arg, TAIL_TOTO_ARG_BYTES, &value)) {
        bytes = 1;
    } else {
        tail_toto_emit_bad_option(arg, '\0');
        return -1;
    }

    if (value == NULL) {
        if (*i + 1 >= argc) {
            tail_toto_emit_missing_arg(bytes ? TAIL_TOTO_ARG_BYTES
                                             : TAIL_TOTO_ARG_LINES, '\0');
            return -1;
        }
        *i += 1;
        value = argv[*i];
    }
    return apply_count(opts, bytes, value);
}

/*
 * Handle one bundle of short flags ("-qv", "-n5", "-qn", ...) at argv[*i];
 * may consume argv[*i + 1] as the value of a trailing -n / -c.
 * Returns 0, or -1 after emitting a diagnostic.
 */
static int parse_short(int argc, char **argv, int *i,
                       struct tail_toto_opts *opts)
{
    const char *arg = argv[*i];
    const char *value;
    size_t j;
    char c;

    for (j = 1; arg[j] != '\0'; j++) {
        c = arg[j];
        int set_mode = 0;

        /* Handle quiet short option -q */
        if (c == SHORT_CHAR(TAIL_TOTO_ARG_QUIET_SHORT)) {
            opts->headers = 0;
            continue;
        }

        /* Handle verbose short option -v */
        if (c == SHORT_CHAR(TAIL_TOTO_ARG_VERBOSE_SHORT)) {
            opts->headers = 1;
            continue;
        } 

        /* Handle lines short option -n or bytes short option -c */
        if (c == SHORT_CHAR(TAIL_TOTO_ARG_LINES_SHORT)
                   || c == SHORT_CHAR(TAIL_TOTO_ARG_BYTES_SHORT)) {
            set_mode = 1;
        }


        if (set_mode) {
            if (arg[j + 1] != '\0') {
                value = arg + j + 1;
            } else if (*i + 1 < argc) {
                *i += 1;
                value = argv[*i];
            } else {
                tail_toto_emit_missing_arg(NULL, c);
                return -1;
            }
            return apply_count(opts,
                               c == SHORT_CHAR(TAIL_TOTO_ARG_BYTES_SHORT),
                               value);
        }
        tail_toto_emit_bad_option(NULL, c);
        return -1;
    }
    return 0;
}

static void init_opts(struct tail_toto_opts *opts)
{
    opts->mode = TAIL_TOTO_MODE_LINES_LAST;
    opts->count = TAIL_TOTO_DEFAULT_COUNT;
    opts->headers = -1;
}

int tail_toto_parse_args(int argc, char **argv, struct tail_toto_opts *opts)
{
    int only_operands = 0;
    int nops = 0;
    int i;
    char *arg;

    init_opts(opts);

    /*
     * Operands are moved down to argv[1 + nops]. nops never exceeds the
     * number of arguments already consumed, so no unread slot is overwritten.
     */
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        /* Handle operands */
        if (only_operands || arg[0] != '-' || strcmp(arg, TAIL_TOTO_ARG_STDIN) == 0) {
            argv[1 + nops] = arg;
            nops++;
            continue;
        } 
        /* Handle "--" */
        if (strcmp(arg, TAIL_TOTO_ARG_END) == 0) {
            only_operands = 1;
            continue;
        } 
        /* Handle long options */
        if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
            if (parse_long(argc, argv, &i, opts) != 0) {
                return -1;
            }
            continue;
        }
        /* Handle short options */
        if (arg[0] == '-' && arg[1] != '-' && arg[1] != '\0') {
            if (parse_short(argc, argv, &i, opts) != 0) {
                return -1;
            }
            continue;
        }
        /* Handle bad options */
        tail_toto_emit_bad_option(arg, '\0');
        return -1;
    }

    if (nops == 0) {
        opts->files = default_files;
        opts->nfiles = 1;
        return 0;
    } 
    opts->files = argv + 1;
    opts->nfiles = nops;
    return 0;
}

/* Flush stdout and report a write error if any stdio output failed. */
static int finish_stdout(void)
{
    if (fflush(stdout) == EOF || ferror(stdout)) {
        tail_toto_emit_write_error();
        return -1;
    }
    return 0;
}

int print_help(void)
{
    fputs("Usage: " TAIL_TOTO_PROGRAM_NAME " [OPTION]... [FILE]...\n"
          "Print the last 10 lines of each FILE to standard output.\n"
          "With more than one FILE, precede each with a header giving the "
          "file name.\n"
          "\n"
          "With no FILE, or when FILE is -, read standard input.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short "
          "options too.\n"
          "  -c, --bytes=[+]NUM       output the last NUM bytes; or use "
          "-c +NUM to\n"
          "                             output starting with byte NUM of "
          "each file\n"
          "  -n, --lines=[+]NUM       output the last NUM lines, instead of "
          "the last 10;\n"
          "                             or use -n +NUM to output starting "
          "with line NUM\n"
          "  -q, --quiet, --silent    never output headers giving file "
          "names\n"
          "  -v, --verbose            always output headers giving file "
          "names\n"
          "      --help, --h          display this help and exit\n"
          "      --version, --v       output version information and exit\n",
          stdout);
    return finish_stdout();
}

int print_version(void)
{
    printf("%s %s\n", TAIL_TOTO_PROGRAM_NAME, TAIL_TOTO_VERSION_STRING);
    return finish_stdout();
}
