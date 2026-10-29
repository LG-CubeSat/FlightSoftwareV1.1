/* Receives, validates, acknowledges, and queues commands addressed to ADCS. */
#ifndef ADCS_COMMUNICATION_COMMAND_HANDLER_H
#define ADCS_COMMUNICATION_COMMAND_HANDLER_H

#include <stdint.h>

#include "communication/message.h"
#include "csp_commands.h"

/* ADCS-owned extensions to the repository's small shared command set. */
typedef enum {
    ADCS_WIRE_COMMAND_SET_MODE = 64, // Numbers chosen mostly randomly. I don't think their documented, either.
    ADCS_WIRE_COMMAND_SET_ATTITUDE = 65,
    ADCS_WIRE_COMMAND_SET_POINTING_VECTOR = 66,
    ADCS_WIRE_COMMAND_RESET_ESTIMATOR = 67,
    ADCS_WIRE_COMMAND_SET_ACTUATOR_INHIBIT = 68,
    ADCS_WIRE_COMMAND_SIMULATOR_FAULT = 69
} adcs_wire_command_id_t;

typedef struct {
    command_envelope_t envelope;
    uint8_t mode;
} adcs_mode_command_payload_t;

typedef struct {
    command_envelope_t envelope;
    versor target_quaternion;
} adcs_attitude_command_payload_t;

typedef struct {
    command_envelope_t envelope;
    float body_axis[ADCS_VECTOR_LENGTH];
    float inertial_direction[ADCS_VECTOR_LENGTH];
} adcs_pointing_command_payload_t;

typedef struct {
    command_envelope_t envelope;
    uint8_t inhibited;
} adcs_actuator_inhibit_payload_t;

typedef struct {
    command_envelope_t envelope;
    uint32_t fault_mask;
} adcs_simulator_fault_payload_t;

/*
 * Starts the CSP listener for ADCS_CMD_PORT. The listener performs only wire
 * decoding and command acknowledgement; mode changes and control work belong
 * to the command task and manager.
 */
void command_handler_init(void);

#endif
