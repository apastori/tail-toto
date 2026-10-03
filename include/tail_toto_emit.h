/*
 * tail_toto_emit.h — stderr diagnostics for tail-toto.
 *
 * Every function writes one GNU-style message to stderr. Functions that
 * report a system error read errno on entry.
 */

#ifndef TAIL_TOTO_EMIT_H
#define TAIL_TOTO_EMIT_H

/* tail-toto: <context>: <strerror(errno)> */
void tail_toto_emit_error(const char *context);

/* tail-toto: cannot open '<name>' for reading: <strerror(errno)> */
void tail_toto_emit_open_error(const char *name);

/* tail-toto: error reading '<name>': <strerror(errno)> */
void tail_toto_emit_read_error(const char *name);

/* tail-toto: write error: <strerror(errno)> */
void tail_toto_emit_write_error(void);

/* tail-toto: invalid number of lines|bytes: '<arg>' */
void tail_toto_emit_invalid_count(int bytes, const char *arg);

/*
 * long_arg != NULL: tail-toto: unrecognized option '<long_arg>'
 * long_arg == NULL: tail-toto: invalid option -- '<short_opt>'
 * Followed by the --help hint line.
 */
void tail_toto_emit_bad_option(const char *long_arg, char short_opt);

/*
 * long_name != NULL: tail-toto: option '<long_name>' requires an argument
 * long_name == NULL: tail-toto: option requires an argument -- '<short_opt>'
 * Followed by the --help hint line.
 */
void tail_toto_emit_missing_arg(const char *long_name, char short_opt);

#endif /* TAIL_TOTO_EMIT_H */
