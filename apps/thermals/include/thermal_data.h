#ifndef THERMAL_DATA_H
#define THERMAL_DATA_H

typedef struct {

    float current_temp;
    float goal_temp;


} ThermalData_t;

void thermals_set_current(float temp);
void thermals_set_goal(float goal);

ThermalData_t get_thermal_data(void);



#endif //THERMAL_DATA_H
