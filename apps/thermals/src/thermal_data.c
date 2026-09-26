
#include <pthread.h>

#include "FreeRTOS.h"
#include "task.h"

#include "thermal_data.h"

/* sensor_read_task, heater_set_task, telemetry_task, and command_task all
   read or write this struct from their own FreeRTOS tasks; the mutex keeps
   get_thermal_data() from ever returning a torn snapshot. */
static pthread_mutex_t thermal_data_lock = PTHREAD_MUTEX_INITIALIZER;

static ThermalData_t thermal_data = {
    .temperatures = {0.00f, 0.00f},
    .valid_sensor_mask = 0u,
    .average_temp = 0.00f,
    .target_temp = 0.00f,
};

/* Caller must hold thermal_data_lock. */
static void thermals_recalculate_average(void)
{
    float sum = 0.0f;
    unsigned int valid_count = 0;

    for (unsigned int i = 0; i < MAX_SENSORS; i++)
    {
        if ((thermal_data.valid_sensor_mask & (1u << i)) != 0u)
        {
            sum += thermal_data.temperatures[i];
            valid_count++;
        }
    }

    if (valid_count > 0)
    {
        thermal_data.average_temp =
            sum / (float)valid_count;
    }
    else
    {
        thermal_data.average_temp = 0.0f;
    }
}

// Sets a sensor's current temperature and marks the reading valid.
void thermals_set_current(float temp, unsigned int sensor_id)
{
    if (sensor_id == 0 || sensor_id > MAX_SENSORS)
    {
        return;
    }

    pthread_mutex_lock(&thermal_data_lock);
    thermal_data.temperatures[sensor_id - 1] = temp;
    thermal_data.valid_sensor_mask |= (1u << (sensor_id - 1));

    thermals_recalculate_average();
    pthread_mutex_unlock(&thermal_data_lock);
}

// Sets the target temperature.
void thermals_set_target(float target)
{
    pthread_mutex_lock(&thermal_data_lock);
    thermal_data.target_temp = target;
    pthread_mutex_unlock(&thermal_data_lock);
}

// Returns a snapshot of the current thermal data.
ThermalData_t get_thermal_data(void)
{
    ThermalData_t snapshot;

    pthread_mutex_lock(&thermal_data_lock);
    snapshot = thermal_data;
    pthread_mutex_unlock(&thermal_data_lock);
    return snapshot;
}

void thermals_invalidate_sensor(unsigned int sensor_id)
{
    if (sensor_id == 0 || sensor_id > MAX_SENSORS)
    {
        return;
    }

    pthread_mutex_lock(&thermal_data_lock);
    thermal_data.valid_sensor_mask &=
        ~(1u << (sensor_id - 1));

    thermals_recalculate_average();
    pthread_mutex_unlock(&thermal_data_lock);
}
