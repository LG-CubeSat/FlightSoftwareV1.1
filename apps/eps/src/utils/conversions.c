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

float eps_conv_mw_to_w(float mw){
    return mw / 1000;
}

float eps_conv_w_to_mw(float w){
    return w * 1000;
}

float eps_conv_j_to_wh(float j){
    return j / 3600;
}

float eps_conv_wh_to_j(float wh){
    return wh * 3600;
}

float eps_conv_c_to_as(float c){  
    return c;
}

float eps_conv_as_to_c(float as){ 
    return as;
}

float eps_conv_mv_to_v(float mv){
    return mv / 1000;
}

float eps_conv_v_to_mv(float v){
    return v * 1000;
}

float eps_conv_ma_to_a(float ma){
    return ma / 1000;
}

float eps_conv_a_to_ma(float a){
    return a * 1000;
}

float eps_conv_kelvin_to_celsius(float kelvin){
    return kelvin - 273.15;
}

float eps_conv_celsius_to_kelvin(float celsius){
    return celsius + 273.15;
}

