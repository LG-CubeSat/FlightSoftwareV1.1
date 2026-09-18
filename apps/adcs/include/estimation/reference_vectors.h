/* Computes the inertial reference vectors observed by ADCS sensors. */
#ifndef ADCS_ESTIMATION_REFERENCE_VECTORS_H
#define ADCS_ESTIMATION_REFERENCE_VECTORS_H

#include "communication/message.h"

/*
 * Computes a unit ECI sun vector from UTC using a low-cost solar ephemeris.
 * The approximation should be documented and bounded before flight use.
 */
adcs_result_t adcs_reference_sun_vector(
    uint64_t unix_time_us,
    float sun_eci_unit[ADCS_VECTOR_LENGTH]);

/*
 * Computes a coarse ECI geomagnetic vector at spacecraft position and time.
 * A dipole model is sufficient for initial simulation; a validated flight
 * implementation can later replace it without changing callers.
 */
adcs_result_t adcs_reference_magnetic_field(
    uint64_t unix_time_us,
    const float position_eci_m[ADCS_VECTOR_LENGTH],
    float magnetic_field_eci_t[ADCS_VECTOR_LENGTH]);

/* Computes the unit vector from the spacecraft toward Earth's centre. */
adcs_result_t adcs_reference_nadir_vector(
    const float position_eci_m[ADCS_VECTOR_LENGTH],
    float nadir_eci_unit[ADCS_VECTOR_LENGTH]);

/* Computes every reference supported by the available time and orbit state. */
adcs_result_t adcs_reference_vectors_compute(
    uint64_t unix_time_us,
    const adcs_orbit_state_t *orbit,
    adcs_reference_vectors_t *references);

#endif
