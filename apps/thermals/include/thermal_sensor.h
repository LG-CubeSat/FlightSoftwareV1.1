

#include <stdint.h>

#ifndef THERMAL_SENSOR_H
#define THERMAL_SENSOR_H



typedef struct 
{
    uint8_t ADDRESS;
    // ADDRESS = HEXADECIMAL I2C ADDRESS OF SENSOR
    uint8_t SENSOR_GENERIC;
    // SENSOR_GENERIC = GENERIC NAME FOR SENSOR.
    // e.g. "001" or "002" etc. This is used to identify which sensor is being used

} thermal_sensor_t;

int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address);
/*
initializes the sensor, must be called at least once before reading 
sets ADDRESS to uint8_t address, see above; and sets 
SENSOR_GENERIC to either the first or second time this function is called
returns positive value if successful,
returns negative if unsuccessful (e.g. address is invalid)
takses in a pointer to thermal_sensor_t struct, along with the address of the sensor, this will initialize
the sensor variable coorolated with the struct with the actual address of the sensor
*/
float thermal_sensor_read(thermal_sensor_t *sensor);

/*
Takes in a pointer to a thermal_sensor_t struct which contains the address of an I2C thermal sensor,
returns a float with the temperature reading in degrees Celsius
returns negative if uncessesful (e.g. sensor not initialized, or address is invalid)
*/




#endif // THERMAL_SENSOR_H