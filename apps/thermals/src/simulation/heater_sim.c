/*
 * Simulation-only heater driver.
 * TODO(HW): provide a separate STM32/I2C heater implementation.
 * Do not compile this file into the hardware target.
 */

 #include "heater_interface.h"

 #include <math.h>
 #include <pthread.h>

 static pthread_mutex_t heater_lock = PTHREAD_MUTEX_INITIALIZER;
 //heater mutex to make sure only one function can change the heater setting at a time

 static float applied_power_fraction = 0.0f;
// 0 = 0% power, 1 = 100% power, .5 = 50% power, etc.

static int heater_initialized = 0;
// 1 = init, 0 = not init

int heater_init(void) {

    pthread_mutex_lock(&heater_lock);
    applied_power_fraction = 0.0f;
    heater_initialized = 1;
    pthread_mutex_unlock(&heater_lock);

    return 1;
    //TODO add return 0 if initialization fails
}

void force_heater_off(void) {

    pthread_mutex_lock(&heater_lock);
    applied_power_fraction = 0.0f; //turn heater off
    pthread_mutex_unlock(&heater_lock);

}

int heater_set_power(float power_fraction) {

    if (!isfinite(power_fraction) || power_fraction < 0 || power_fraction > 1) {
        force_heater_off();
        return 0;
    }

    pthread_mutex_lock(&heater_lock);

    if (heater_initialized == 0) {
        pthread_mutex_unlock(&heater_lock);
        return 0;
    }

    applied_power_fraction = power_fraction;

    pthread_mutex_unlock(&heater_lock);

    return 1;

}

float get_applied_power(void) {

    float applied_power;

    pthread_mutex_lock(&heater_lock);
    applied_power = applied_power_fraction;
    pthread_mutex_unlock(&heater_lock);

    return applied_power;

}
//uses temporary variable to store actual variable (this way the mutex will be able to unlock) and returns it
