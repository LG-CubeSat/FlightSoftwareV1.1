#ifndef THERMALS_HEATER_INTERFACE_H
#define THERMALS_HEATER_INTERFACE_H


int heater_init(void);
// initializes the heater
// returns 1 = succsess, 0 = fail

int heater_set_power(float power_fraction);
// @param power_fraction = 0.0 - 1.0 range, (e.g .25 = 25% power, 1 = 100% power, 0 = 0% power)
// returns 1 = accepted, 0 = rejected / failed

void force_heater_off(void);
// in an emergency situation, or just one where we need the heater off, this ovverides 
// any target value and immediatly sets the heater to 0% power

float get_applied_power(void);
// returns what the driver believes it is applying to the heater


#endif //THERMALS_HEATER_INTERFACE_H