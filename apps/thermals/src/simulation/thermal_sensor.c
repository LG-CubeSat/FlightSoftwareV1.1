/*
 * Simulation temperature-sensor implementation.
 *
 * TODO(HW): replace this source file with an STM32/I2C sensor driver
 * in the hardware target.
 */

#include "thermal_sensor.h"

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include "simulation/thermal_model.h"

#define SENSOR_1_SIM_OFFSET_C (-0.10f)
#define SENSOR_2_SIM_OFFSET_C (0.10f)

static uint8_t sensor_count = 0;

int thermal_sensor_init(thermal_sensor_t *sensor, uint8_t address)
{
    if (sensor == NULL)
    {
        return -1;
    }

    if (address > 0x7F)
    {
        return -1; // invalid address
    }
    if (sensor_count >= MAX_SENSORS)
    {
        return -2; // two sensors (MAX) already initialized
    }

    sensor->address = address;
    sensor_count++;
    return 1; // successful initialization
}
/*
 * Reads the specified thermal sensor.
 *
 * On success:
 *   - stores the temperature in degrees Celsius at temperature_out
 *   - returns 1
 *
 * On failure:
 *   - does not provide a valid temperature
 *   - returns a negative error code
 */
int thermal_sensor_read(
    const thermal_sensor_t *sensor,
    float *temperature_out)
{
    float model_temperature_c;

    if (sensor == NULL || temperature_out == NULL)
    {
        return -1;
    }

    model_temperature_c = thermal_model_get_temperature();

    if (!isfinite(model_temperature_c))
    {
        return -2;
    }

    if (sensor->sensor_id == 1u)
    {
        *temperature_out =
            model_temperature_c + SENSOR_1_SIM_OFFSET_C;

        return 1;
    }

    if (sensor->sensor_id == 2u)
    {
        *temperature_out =
            model_temperature_c + SENSOR_2_SIM_OFFSET_C;

        return 1;
    }

    return -3;
}

/*
-1 = invalid pointer
-2 = thermal model produced an invalid temperature
-3 = unknown sensor ID
 1 = successful sensor reading
*/