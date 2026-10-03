/*
 * tail_toto_tail.h — tail algorithms over file descriptors.
 */

#ifndef TAIL_TOTO_TAIL_H
#define TAIL_TOTO_TAIL_H

#include "tail_toto.h"

enum tail_toto_status {
    TAIL_TOTO_OK = 0,
    TAIL_TOTO_READ_ERR,
    TAIL_TOTO_WRITE_ERR
};

/*
 * Copy the part of in_fd selected by opts->mode and opts->count to out_fd.
 *
 * Seekable regular files are scanned backwards from the end with a stack
 * buffer; other inputs are streamed forward and buffered on the heap.
 * Reading starts at the current offset of in_fd.
 *
 * Returns: TAIL_TOTO_OK, TAIL_TOTO_READ_ERR, or TAIL_TOTO_WRITE_ERR with
 *          errno preserved from the failing call. Never exits, never prints
 *          diagnostics, frees all heap memory before returning.
 */
enum tail_toto_status tail_toto_tail_fd(int in_fd, int out_fd,
                                        const struct tail_toto_opts *opts);

#endif /* TAIL_TOTO_TAIL_H */
