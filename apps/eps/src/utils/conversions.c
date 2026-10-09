#include "utils/conversions.h"

float eps_watts_to_milliwatts(float watts) {
    return watts * 1000.0F;
}

float eps_milliwatts_to_watts(float milliwatts) {
    return milliwatts / 1000.0F;
}

float eps_joules_to_watt_hours(float joules) {
    return joules / 3600.0F;
}

float eps_watt_hours_to_joules(float watt_hours) {
    return watt_hours * 3600.0F;
}

float eps_coulombs_to_ampere_seconds(float coulombs) {
    return coulombs;
}

float eps_ampere_seconds_to_coulombs(float ampere_seconds) {
    return ampere_seconds;
}

float eps_volts_to_millivolts(float volts) {
    return volts * 1000.0F;
}

float eps_millivolts_to_volts(float millivolts) {
    return millivolts / 1000.0F;
}

float eps_amperes_to_milliamperes(float amperes) {
    return amperes * 1000.0F;
}

float eps_milliamperes_to_amperes(float milliamperes) {
    return milliamperes / 1000.0F;
}

float eps_kelvin_to_celsius(float kelvin) {
    return kelvin - 273.15F;
}

float eps_celsius_to_kelvin(float celsius) {
    return celsius + 273.15F;
}
