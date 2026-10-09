/*
 * Small, explicit unit conversions used across the EPS.
 *
 * These are deliberately plain functions rather than macros so they show up
 * in debuggers and can be unit tested. Naming is <from>_to_<to>.
 */
#ifndef EPS_UTILS_CONVERSIONS_H
#define EPS_UTILS_CONVERSIONS_H

#include <stdint.h>

float eps_watts_to_milliwatts(float watts);
float eps_milliwatts_to_watts(float milliwatts);

float eps_joules_to_watt_hours(float joules);
float eps_watt_hours_to_joules(float watt_hours);

float eps_coulombs_to_ampere_seconds(float coulombs);
float eps_ampere_seconds_to_coulombs(float ampere_seconds);

float eps_volts_to_millivolts(float volts);
float eps_millivolts_to_volts(float millivolts);

float eps_amperes_to_milliamperes(float amperes);
float eps_milliamperes_to_amperes(float milliamperes);

float eps_kelvin_to_celsius(float kelvin);
float eps_celsius_to_kelvin(float celsius);

#endif
