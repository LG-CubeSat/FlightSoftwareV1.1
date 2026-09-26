#include "prop_int_controller.h"

#include <stdio.h>
#include <stdlib.h>

#define TOLERANCE (0.5) //MAXIMUM DIFFERENCE BETWEEN GOAL TEMP AND CURRENT TEMP (CAN BE CHANGED)
#define RESPONSE (.1) //# of SECONDS it takes for the heater to adjust to new temperature (Can be changed)

float PI_CONTROLLER(float current_temp, float goal_temp) {

    float dt = 0.1;
    float kP = 0.4;
    float kI = 0.2;

    float integral = 0;

    float error;
    float output;

    float prop_term;
    float integ_term;

    float formula = 1; //1 is placeholder, once we have heater pelase fill this in with degrees * formula = volts needed

    float setting = 0; 
    /*This is very important, this is the variable that is going to be returned and it represents
    the amount of voltage (VOLTS) that the heater is going to recieve, functions above need to be implemented 
    w/ hardware to get the correct voltage formlua. (e.g. setting (volts) = 2, this should set the temperature to 25, the 
    formula may be volts = setting * 12.5)*/


    while (abs(error) > TOLERANCE) {

        error = goal_temp - current_temp;
        integral += error * dt;

        prop_term = error * kP;
        integ_term = integral * kI;
    
        output = prop_term + integ_term;

        current_temp += output;
    }

    /*
    STEPS:
    -Loss is calculated
    -Integral variable is updated to keep running count of changes
    -Proportional term is set to error * constant P
    -Integral term is set to integral * constant I
    -output is saved as Prop_term + Integ_term
    -Current_temp is updated

    -ALL OF THE ABOVE RUNS UNTIL LOSS IS LESS THAN TOLERANCE CONSTANT (SEE TOP)

    -After loss is below tolerance constant, the setting variable is returned
    */

    return (setting * formula);


}