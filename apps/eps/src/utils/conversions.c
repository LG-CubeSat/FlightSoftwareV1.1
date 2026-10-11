/*
 * Include conversions for alot of things including:
 *watts and milliwatts,
joules and watt-hours,
coulombs and ampere-seconds,
volts and millivolts,
amperes and milliamperes,
Kelvin and Celsius.
*/

#include "utils/conversions.h"

float eps_conv_mw_to_w(float mw) {
    return mw * 1000;
}
