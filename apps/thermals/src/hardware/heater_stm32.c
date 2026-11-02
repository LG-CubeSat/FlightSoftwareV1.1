#include "heater_interface.h"

static float applied_heater_power = 0f;


int heater_init(void) {

    if (/* set power to 0 */) {
        return 1;
    }

    else return -1;
}

int heater_set_power(float power_fraction) {

    if (power_fraction < 0 || power_fraction > 100) {

        force_heater_off();
        return -1;
    }
    else {
        applied_heater_power = power_fraction;
        //set the power to the applied_heater_power

    }

}

void force_heater_off(void) {

    /*code to set heater power to 0, do not use heater_set_power the code should be manual here; this is
     becuase if there is an error with heater_set_power it will call force_heater_off so we don't want to be going back and forth
    */
   //turn heater off here
   applied_heater_power = 0;

}

float get_applied_power(void) {
    return applied_heater_power;
}