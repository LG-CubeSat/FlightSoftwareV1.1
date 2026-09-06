/*
Shared CSP node addresses, ports, and wire structs for the
OBC <-> ADCS position-command integration.

Port numbers match docs/architecture.md (ADCS command = 10,
ADCS telemetry = 20).
*/
#ifndef CSP_COMMANDS_H
#define CSP_COMMANDS_H

#include <stdint.h>

#define OBC_ADDRESS      1
#define ADCS_ADDRESS     2
#define EPS_ADDRESS      3
#define THERMALS         4
#define CAMERA           5
#define COMMS            6

// MAKE SURE TO USE PORTS OUTLINED IN README. DONT MAKE THEM UP!
#define ADCS_CMD_PORT    10
#define ADCS_TELEM_PORT  20

#define EPS_CMD_PORT 11
#define EPS_TELEM_PORT 21

#define ADCS_STATUS_PORT 25 // board-initiated reset notices, telemetry range (20-29)
#define TIME_SYNC_REQUEST_PORT 26

typedef struct {
    uint8_t command_id;
    uint32_t seq;
} command_envelope_t;

/* Here is where you add more CMDs */
typedef enum {
    CMD_MOVE_TO_POSITION = 1, // TODO: remove, its studded
    CMD_RESET = 2,
    CMD_SHUTDOWN = 3,
    CMD_TIME_SYNC = 4,
    // ADCS COMMANDS
    CMD_POINT_TO_SUN = 5
} command_id_t;

// ------

typedef enum {
    RESET_REASON_OUT_OF_BOUNDS = 1,
    RESET_REASON_WATCHDOG = 2
} reset_reason_t;

/* sent to OBC from MCU, port ADCS_STATUS_PORT. Fire-and-forget: sent after a
   board has already reset itself, not a request for permission. */
typedef struct {
    uint8_t board_addr; // e.g. ADCS_ADDRESS -- which board this is about
    uint8_t reason;      // reset_reason_t
} board_reset_notice_t;

// what the status of ack is
typedef enum {
    ACK = 0,
    NACK = 1
} command_ack_status_t;

typedef struct {
    uint8_t ack_command_id;
    uint32_t ack_seq;
    command_ack_status_t status;
} command_ack_t;

/* OBC -> ADCS, port ADCS_CMD_PORT */
typedef struct {
    command_envelope_t envelope;
    int32_t target_position;
} position_command_t;

/* ADCS -> OBC, port ADCS_TELEM_PORT */
typedef struct {
    int32_t current_position;
} position_telemetry_t;

typedef struct {
    command_envelope_t envelope;
    int64_t unix_time_sec;
} time_sync_command_t; // OBC -> board, sent to the board's own CMD Port


#endif
