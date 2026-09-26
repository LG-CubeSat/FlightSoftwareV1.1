#ifndef PROP_INT_CONTROLLER_H
#define PROP_INT_CONTROLLER_H

float PI_CONTROLLER(float current_temp, float goal_temp);
/*
Basic Proportional-Integral Controller (No derivative path!)
takes in current_temp and goal_temp and outputs temp heater should be set to
*/


#endif // PROP_INT_CONTROLLER_H