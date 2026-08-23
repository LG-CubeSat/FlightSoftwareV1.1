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

typedef enum {
    CMD_MOVE_TO_POSITION = 1
} command_id_t;

/* OBC -> ADCS, port ADCS_CMD_PORT */
typedef struct {
    int32_t target_position;
} position_command_t;

/* ADCS -> OBC, port ADCS_TELEM_PORT */
typedef struct {
    int32_t current_position;
} position_telemetry_t;

#endif
