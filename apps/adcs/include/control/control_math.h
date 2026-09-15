/* Small, testable math primitives shared by ADCS control laws. */
#ifndef ADCS_CONTROL_CONTROL_MATH_H
#define ADCS_CONTROL_CONTROL_MATH_H

#include <stddef.h>
#include <stdint.h>

#include "communication/message.h"

typedef struct {
    float proportional_gain;
    float integral_gain;
    float derivative_gain;
    float integrator_limit;
    float output_limit;
} adcs_pid_config_t;

typedef struct {
    adcs_pid_config_t config;
    float integral;
    float previous_error;
    uint8_t has_previous_error;
} adcs_pid_state_t;

/* Loads PID gains and clears all controller history. */
void adcs_pid_init(adcs_pid_state_t *state, const adcs_pid_config_t *config);

/* Clears accumulated integral and derivative history without changing gains. */
void adcs_pid_reset(adcs_pid_state_t *state);

/* Advances a scalar PID controller by one positive, finite time step. */
adcs_result_t adcs_pid_update(
    adcs_pid_state_t *state,
    float error,
    float dt_s,
    float *output);

/* Restricts a scalar to the inclusive [minimum, maximum] interval. */
float adcs_clampf(float value, float minimum, float maximum);

/* Computes the Euclidean norm of a three-element vector. */
float adcs_vector_norm(const float vector[ADCS_VECTOR_LENGTH]);

/* Normalizes a vector and rejects zero, non-finite, or near-zero inputs. */
adcs_result_t adcs_vector_normalize(
    const float input[ADCS_VECTOR_LENGTH],
    float output[ADCS_VECTOR_LENGTH]);

/* Computes the right-handed three-dimensional cross product left x right. */
void adcs_vector_cross(
    const float left[ADCS_VECTOR_LENGTH],
    const float right[ADCS_VECTOR_LENGTH],
    float output[ADCS_VECTOR_LENGTH]);

/* Scales a vector, preserving direction, so its norm does not exceed limit. */
adcs_result_t adcs_vector_limit(
    const float input[ADCS_VECTOR_LENGTH],
    float limit,
    float output[ADCS_VECTOR_LENGTH],
    uint8_t *was_limited);

/* Normalizes a local [x, y, z, w] quaternion and rejects degenerate inputs. */
adcs_result_t adcs_quaternion_normalize(
    const versor input,
    versor output);

/*
 * Produces the shortest-path physical body rotation, expressed in the current
 * body frame, from current attitude to target attitude.
 */
adcs_result_t adcs_quaternion_error_vector(
    const versor current,
    const versor target,
    float error[ADCS_VECTOR_LENGTH]);

/* Reports whether every element in an array is finite. */
uint8_t adcs_values_are_finite(const float *values, size_t count);

#endif
