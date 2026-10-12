// See conversions.c

#ifndef EPS_UTILS_CONVERSIONS_H
#define EPS_UTILS_CONVERSIONS_H

float eps_conv_mw_to_w(float mw);
float eps_conv_w_to_mw(float w);
float eps_conv_j_to_wh(float j);
float eps_conv_wh_to_j(float wh);
float eps_conv_c_to_as(float c);
float eps_conv_as_to_c(float as);
float eps_conv_mv_to_v(float mv);
float eps_conv_v_to_mv(float v);
float eps_conv_ma_to_a(float ma);
float eps_conv_a_to_ma(float a);
float eps_conv_kelvin_to_celsius(float kelvin);
float eps_conv_celsius_to_kelvin(float celsius);

#endif