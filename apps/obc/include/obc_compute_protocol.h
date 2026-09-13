#ifndef OBC_COMPUTE_PROTOCOL_H
#define OBC_COMPUTE_PROTOCOL_H

#include <stdint.h>

#define COMPUTE_MAX_PATH 64

typedef enum {
    COMPUTE_STATUS_OK = 0,
    COMPUTE_STATUS_FAILED = 1,
    COMPUTE_STATUS_CANCELLED = 2,
    COMPUTE_STATUS_BUSY = 3   // rejected: a job is already running
} compute_status_t;

/* role -> ROLE_COMPUTE: compress in_path, write result to out_path.
   sample_width is 1, 2, or 4 -- the native width of the data being
   compressed (1 for a raw byte/image stream, 2 or 4 for a fixed-width
   telemetry/sensor sample stream). */
typedef struct {
    uint32_t job_id;
    char in_path[COMPUTE_MAX_PATH];
    char out_path[COMPUTE_MAX_PATH];
    uint8_t sample_width;
} compute_compress_request_t;

/* role -> ROLE_COMPUTE: stop job_id if it's still running (ignored
   otherwise -- nothing polls for a cancel ack) */
typedef struct {
    uint32_t job_id;
} compute_cancel_request_t;

/* ROLE_COMPUTE -> requester, fire-and-forget: immediately for a BUSY
   rejection, or once when the job finishes/fails/cancels */
typedef struct {
    uint32_t job_id;
    compute_status_t status;
    uint32_t output_size; // valid only when status == COMPUTE_STATUS_OK
} compute_result_t;

#endif
