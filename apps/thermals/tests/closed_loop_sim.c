#include <math.h>
#include <stdio.h>

#include "heater_interface.h"
#include "prop_int_controller.h"
#include "simulation/thermal_model.h"
#include "thermal_sensor.h"

#define TEST_TARGET_TEMPERATURE_C  (50.0f)
#define TEST_TIME_STEP_SECONDS     (0.1f)
#define TEST_SIMULATION_STEPS      (10000)

#define TEST_PI_KP (0.251f)
#define TEST_PI_KI (0.004f)
int main(void)
{
    thermal_sensor_t sensor_1 = {
        .address = 0x48u,
        .sensor_id = 1u
    };

    thermal_sensor_t sensor_2 = {
        .address = 0x49u,
        .sensor_id = 2u
    };

    float sensor_1_temp;
    float sensor_2_temp;
    float average_temp;
    float requested_power;
    float final_temperature;

    if (!heater_init() ||
        !thermal_model_init() ||
        !prop_int_controller_init(TEST_PI_KP, TEST_PI_KI) ||
        thermal_sensor_init(&sensor_1, sensor_1.address) < 0 ||
        thermal_sensor_init(&sensor_2, sensor_2.address) < 0)
    {
        printf("[CLOSED_LOOP_TEST] Initialization failed.\n");
        force_heater_off();
        return 1;
    }

    printf(
        "[CLOSED_LOOP_TEST] Starting target=%.2f C\n",
        TEST_TARGET_TEMPERATURE_C
    );

    for (int step = 0; step < TEST_SIMULATION_STEPS; step++)
    {
        if (thermal_sensor_read(&sensor_1, &sensor_1_temp) < 0 ||
            thermal_sensor_read(&sensor_2, &sensor_2_temp) < 0)
        {
            printf("[CLOSED_LOOP_TEST] Sensor read failed.\n");
            force_heater_off();
            return 1;
        }

        average_temp = (sensor_1_temp + sensor_2_temp) / 2.0f;

        if (!prop_int_controller_update(
                TEST_TARGET_TEMPERATURE_C,
                average_temp,
                TEST_TIME_STEP_SECONDS,
                &requested_power))
        {
            printf("[CLOSED_LOOP_TEST] Controller update failed.\n");
            force_heater_off();
            return 1;
        }

        if (!heater_set_power(requested_power))
        {
            printf("[CLOSED_LOOP_TEST] Heater update failed.\n");
            force_heater_off();
            return 1;
        }

        if (!thermal_model_step(
                get_applied_power(),
                TEST_TIME_STEP_SECONDS))
        {
            printf("[CLOSED_LOOP_TEST] Thermal-model update failed.\n");
            force_heater_off();
            return 1;
        }

        if ((step % 100) == 0)
        {
            printf(
                "time=%6.1f  temp=%6.2f C  target=%6.2f C  power=%5.1f%%\n",
                step * TEST_TIME_STEP_SECONDS,
                average_temp,
                TEST_TARGET_TEMPERATURE_C,
                get_applied_power() * 100.0f
            );
        }
    }

    final_temperature = thermal_model_get_temperature();

    force_heater_off();

    if (!isfinite(final_temperature))
    {
        printf("[CLOSED_LOOP_TEST] Final temperature is invalid.\n");
        return 1;
    }

    printf(
        "[CLOSED_LOOP_TEST] Finished final=%.2f C error=%.2f C\n",
        final_temperature,
        TEST_TARGET_TEMPERATURE_C - final_temperature
    );

    return 0;
}