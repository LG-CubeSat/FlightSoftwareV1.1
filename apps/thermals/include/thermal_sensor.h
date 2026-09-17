#include <stdint.h>


#ifndef THERMAL_SENSOR_H
#define THERMAL_SENSOR_H

typedef struct
{
   uint8_t address;
   // address = HEXADECIMAL I2C ADDRESS OF SENSOR
   uint8_t sensor_id;
   // sensor_id = GENERIC NAME FOR SENSOR.
   // e.g. "001" or "002" etc. This is used to identify which sensor is being used

} thermal_sensor_t;


int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address);
/*
initializes the sensor, must be called at least once before reading
sets address to uint8_t address, see above; and sets
sensor_id to either the first or second time this function is called
returns positive value if successful,
returns negative if unsuccessful (e.g. address is invalid)
takes in a pointer to thermal_sensor_t struct, along with the address of the sensor, this will initialize
the sensor variable correlated with the struct with the actual address of the sensor
*/
int thermal_sensor_read(const thermal_sensor_t *sensor, float *temperature_out);


/*
Takes in a pointer to a thermal_sensor_t struct which contains the address of an I2C thermal sensor,
returns a float with the temperature reading in degrees Celsius
returns negative if unsuccessful (e.g. sensor not initialized, or address is invalid)
*/


#endif // THERMAL_SENSOR_H
