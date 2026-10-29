/* Board pinout/parts for these devices are not configured in the .ioc yet.
 * Explicit unavailable responses keep validity bits clear and actuators inhibited.
 * Replace this file with measured/calibrated board drivers when hardware is known.
 */
#include "magnetometer.h"
#include "sun_sensor.h"
#include "magnetorquer.h"
#include <string.h>

magnetometer_status_t magnetometer_initialize(void) { return MAGNETOMETER_NOT_READY; }
magnetometer_status_t magnetometer_read(magnetometer_sample_t *sample) {
    if (sample != NULL) memset(sample, 0, sizeof(*sample));
    return MAGNETOMETER_NOT_READY;
}
sun_sensor_status_t sun_sensor_initialize(void) { return SUN_SENSOR_NOT_READY; }
sun_sensor_status_t sun_sensor_read(sun_sensor_sample_t *sample) {
    if (sample != NULL) memset(sample, 0, sizeof(*sample));
    return SUN_SENSOR_NOT_READY;
}
magnetorquer_status_t magnetorquer_initialize(void) { return MAGNETORQUER_NOT_READY; }
magnetorquer_status_t magnetorquer_set(const magnetorquer_command_t *command) {
    (void)command;
    return MAGNETORQUER_NOT_READY;
}
magnetorquer_status_t magnetorquer_disable(void) { return MAGNETORQUER_NOT_READY; }
magnetorquer_status_t magnetorquer_get_last_command(magnetorquer_command_t *command) {
    if (command != NULL) memset(command, 0, sizeof(*command));
    return MAGNETORQUER_NOT_READY;
}
