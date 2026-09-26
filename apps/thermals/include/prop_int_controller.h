#ifndef PROP_INT_CONTROLLER_H
#define PROP_INT_CONTROLLER_H

#include <stdio.h>
#include <stdlib.h>

void __PI_INIT__(void);
//inits integral

float PI_CONTROLLER_UPDATE(float current_temp, float target_temp);
/*
Basic Proportional-Integral Controller (No derivative path!)
takes in current_temp and goal_temp and outputs temp heater should be set to
*/




#endif // PROP_INT_CONTROLLER_H