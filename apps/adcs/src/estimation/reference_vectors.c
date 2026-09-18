#include "estimation/reference_vectors.h"

#include <math.h>
#include <string.h>

#include "control/control_math.h"

#define ADCS_PI_F 3.14159265358979323846F
#define ADCS_DEG_TO_RAD (ADCS_PI_F / 180.0F)
#define ADCS_EARTH_RADIUS_M 6371000.0F
#define ADCS_EARTH_DIPOLE_FIELD_FACTOR 7.94e15F
#define ADCS_SIDEREAL_DAY_S 86164.0905
#define ADCS_REFERENCE_MIN_MODEL_ALTITUDE_M 100000.0F
#define ADCS_REFERENCE_MAX_MODEL_ALTITUDE_M 2000000.0F
#define ADCS_CUBE(value) ((value) * (value) * (value))
/* The sanity band is derived from the dipole factor over the supported
   100--2000 km model envelope, with a fourfold orientation/model margin. */
#define ADCS_REFERENCE_MIN_FIELD_T \
    (0.25F * ADCS_EARTH_DIPOLE_FIELD_FACTOR / \
     ADCS_CUBE(ADCS_EARTH_RADIUS_M + ADCS_REFERENCE_MAX_MODEL_ALTITUDE_M))
#define ADCS_REFERENCE_MAX_FIELD_T \
    (4.0F * ADCS_EARTH_DIPOLE_FIELD_FACTOR / \
     ADCS_CUBE(ADCS_EARTH_RADIUS_M + ADCS_REFERENCE_MIN_MODEL_ALTITUDE_M))

adcs_result_t adcs_reference_sun_vector(
    uint64_t unix_time_us,
    float sun_eci_unit[ADCS_VECTOR_LENGTH]) {
    double julian_day;
    double days_since_j2000;
    double mean_longitude_deg;
    double mean_anomaly_rad;
    double ecliptic_longitude_rad;
    double obliquity_rad;
    float vector[ADCS_VECTOR_LENGTH];

    if (sun_eci_unit == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    if (unix_time_us == 0U) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    julian_day = (double)unix_time_us / 86400000000.0 + 2440587.5;
    days_since_j2000 = julian_day - 2451545.0;
    mean_longitude_deg = fmod(280.460 + 0.9856474 * days_since_j2000, 360.0);
    mean_anomaly_rad = fmod(
        357.528 + 0.9856003 * days_since_j2000,
        360.0) * (double)ADCS_DEG_TO_RAD;
    ecliptic_longitude_rad =
        (mean_longitude_deg + 1.915 * sin(mean_anomaly_rad) +
         0.020 * sin(2.0 * mean_anomaly_rad)) * (double)ADCS_DEG_TO_RAD;
    obliquity_rad =
        (23.439 - 0.0000004 * days_since_j2000) * (double)ADCS_DEG_TO_RAD;

    vector[0] = (float)cos(ecliptic_longitude_rad);
    vector[1] = (float)(cos(obliquity_rad) * sin(ecliptic_longitude_rad));
    vector[2] = (float)(sin(obliquity_rad) * sin(ecliptic_longitude_rad));
    return adcs_vector_normalize(vector, sun_eci_unit);
}

adcs_result_t adcs_reference_magnetic_field(
    uint64_t unix_time_us,
    const float position_eci_m[ADCS_VECTOR_LENGTH],
    float magnetic_field_eci_t[ADCS_VECTOR_LENGTH]) {
    float radial_unit[ADCS_VECTOR_LENGTH];
    float dipole_axis[ADCS_VECTOR_LENGTH];
    float radius;
    float projection = 0.0F;
    float scale;
    double earth_angle;
    float field_norm;

    if (position_eci_m == NULL || magnetic_field_eci_t == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    if (unix_time_us == 0U) {
        return ADCS_RESULT_UNAVAILABLE;
    }
    if (!adcs_values_are_finite(position_eci_m, ADCS_VECTOR_LENGTH)) {
        return ADCS_RESULT_INVALID_DATA;
    }

    radius = adcs_vector_norm(position_eci_m);
    if (!isfinite(radius) || radius <= ADCS_EARTH_RADIUS_M ||
        adcs_vector_normalize(position_eci_m, radial_unit) != ADCS_RESULT_OK) {
        memset(magnetic_field_eci_t, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
        return ADCS_RESULT_OUT_OF_RANGE;
    }

    earth_angle = fmod(
        (double)unix_time_us / 1000000.0,
        ADCS_SIDEREAL_DAY_S) * (2.0 * (double)ADCS_PI_F / ADCS_SIDEREAL_DAY_S);
    dipole_axis[0] = sinf(11.0F * ADCS_DEG_TO_RAD) * (float)cos(earth_angle);
    dipole_axis[1] = sinf(11.0F * ADCS_DEG_TO_RAD) * (float)sin(earth_angle);
    dipole_axis[2] = cosf(11.0F * ADCS_DEG_TO_RAD);

    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        projection += dipole_axis[axis] * radial_unit[axis];
    }
    scale = ADCS_EARTH_DIPOLE_FIELD_FACTOR / (radius * radius * radius);
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        magnetic_field_eci_t[axis] = scale *
            (3.0F * projection * radial_unit[axis] - dipole_axis[axis]);
    }

    field_norm = adcs_vector_norm(magnetic_field_eci_t);
    if (!isfinite(field_norm) ||
        field_norm < ADCS_REFERENCE_MIN_FIELD_T ||
        field_norm > ADCS_REFERENCE_MAX_FIELD_T) {
        memset(magnetic_field_eci_t, 0, sizeof(float) * ADCS_VECTOR_LENGTH);
        return ADCS_RESULT_OUT_OF_RANGE;
    }
    return ADCS_RESULT_OK;
}

adcs_result_t adcs_reference_nadir_vector(
    const float position_eci_m[ADCS_VECTOR_LENGTH],
    float nadir_eci_unit[ADCS_VECTOR_LENGTH]) {
    float toward_earth[ADCS_VECTOR_LENGTH];

    if (position_eci_m == NULL || nadir_eci_unit == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }
    for (size_t axis = 0U; axis < ADCS_VECTOR_LENGTH; ++axis) {
        toward_earth[axis] = -position_eci_m[axis];
    }
    return adcs_vector_normalize(toward_earth, nadir_eci_unit);
}

adcs_result_t adcs_reference_vectors_compute(
    uint64_t unix_time_us,
    const adcs_orbit_state_t *orbit,
    adcs_reference_vectors_t *references) {
    if (references == NULL) {
        return ADCS_RESULT_INVALID_ARGUMENT;
    }

    memset(references, 0, sizeof(*references));
    references->unix_time_us = unix_time_us;
    if (unix_time_us == 0U) {
        return ADCS_RESULT_UNAVAILABLE;
    }

    if (adcs_reference_sun_vector(
            unix_time_us,
            references->sun_eci_unit) == ADCS_RESULT_OK) {
        references->valid_mask |= ADCS_REFERENCE_VALID_SUN;
    }

    if (orbit != NULL && orbit->valid != 0U) {
        if (adcs_reference_magnetic_field(
                unix_time_us,
                orbit->position_eci_m,
                references->magnetic_field_eci_t) == ADCS_RESULT_OK) {
            references->valid_mask |= ADCS_REFERENCE_VALID_MAGNETIC_FIELD;
        }
        if (adcs_reference_nadir_vector(
                orbit->position_eci_m,
                references->nadir_eci_unit) == ADCS_RESULT_OK) {
            references->valid_mask |= ADCS_REFERENCE_VALID_NADIR;
        }
    }

    return references->valid_mask == ADCS_REFERENCE_VALID_NONE
        ? ADCS_RESULT_UNAVAILABLE
        : ADCS_RESULT_OK;
}
