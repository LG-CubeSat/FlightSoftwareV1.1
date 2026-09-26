#include "prop_int_controller.h"

#include <stdio.h>
#include <math.h>

#define TOLERANCE (0.5f) //MAXIMUM DIFFERENCE BETWEEN GOAL TEMP AND CURRENT TEMP (CAN BE CHANGED)
#define CONTROLLER_DT_SECONDS (0.1f) //# of SECONDS it takes for the heater to adjust to new temperature (Can be changed)
#define HEATER_MAX (15) //MAX VOLTS HEATER CAN HANDLE 15 is placeholder

#define KP (0.4f)
#define KI (0.2f)

static float integral = 0.0f;
static int initialized = 0; 

void __PI_INIT__(void) {
    if (initialized == 0) {
    
        integral = 0.0f;
        initialized = 1;

    }
}

float PI_CONTROLLER_UPDATE(float current_temp, float target_temp) {

    float error;

    float prop_term;
    float integ_term;

    float output = 0; 
    /*This is very important, this is the variable that is going to be returned and it represents
    the amount of voltage (VOLTS) that the heater is going to recieve, functions above need to be implemented 
    w/ hardware to get the correct voltage formlua. (e.g. output (volts) = 2, this should set the temperature to 25, the 
    formula may be volts = output * 12.5)*/

        error = target_temp - current_temp;
        integral += error * CONTROLLER_DT_SECONDS;

        prop_term = error * KP;
        integ_term = integral * KI;
    
        output = prop_term + integ_term;

        if (output < 0) {
            //volts cannot be negative
            output = 0;
        }
        if (output > HEATER_MAX) {
            //cieling protection
            output = HEATER_MAX;
        }

        return output;
}