**control_task.h**
__________________
#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Create and initialize the Control Task.
 */
void control_task_init(void);

/**
 * @brief FreeRTOS Control Task.
 *
 * Waits for attitude estimates and computes actuator commands.
 */
void control_task(void *pvParameters);

#endif
__________________

**control_task.c**
#include "tasks/control_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

/* ADCS Modules */
#include "manager/adcs_manager.h"
#include "control/controller.h"

/* Communication */
#include "communication/adcs_messages.h"

/* Drivers */
#include "drivers/magnetorquer.h"

/**************************************************************
 * Configuration
 **************************************************************/

#define CONTROL_TASK_PRIORITY      (3)
#define CONTROL_TASK_STACK_SIZE    (1024)
#define CONTROL_TASK_PERIOD_MS     (20)

/**************************************************************
 * Private Variables
 **************************************************************/

static TaskHandle_t controlTaskHandle = NULL;

/* Queue Handles (obtained during initialization) */
static QueueHandle_t attitudeQueue = NULL;

/**************************************************************
 * Private Function Prototypes
 **************************************************************/

static void receive_attitude(void);

static void determine_control_mode(void);

static void compute_control(void);

static void send_actuator_commands(void);

/**************************************************************
 * Initialization
 **************************************************************/

void control_task_init(void)
{
    /* Obtain Queue Handles */

    // attitudeQueue = communication_get_attitude_queue();

    /* Initialize Controller */

    // controller_init();

    /* Create FreeRTOS Task */

    xTaskCreate(
        control_task,
        "Control",
        CONTROL_TASK_STACK_SIZE,
        NULL,
        CONTROL_TASK_PRIORITY,
        &controlTaskHandle
    );
}

/**************************************************************
 * Task
 **************************************************************/

void control_task(void *pvParameters)
{
    (void)pvParameters;

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /******************************************************
         * 1. Receive Latest Attitude Estimate
         ******************************************************/

        receive_attitude();

        /******************************************************
         * 2. Determine Active Control Mode
         ******************************************************/

        determine_control_mode();

        /******************************************************
         * 3. Compute Desired Control Output
         ******************************************************/

        compute_control();

        /******************************************************
         * 4. Send Commands to Magnetorquers
         ******************************************************/

        send_actuator_commands();

        /******************************************************
         * 5. Wait Until Next Control Cycle
         ******************************************************/

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS)
        );
    }
}

/**************************************************************
 * Private Functions
 **************************************************************/

static void receive_attitude(void)
{
    // Wait for newest attitude estimate

    // xQueueReceive(...)
}

static void determine_control_mode(void)
{
    // Ask ADCS manager what mode we are in

    // SAFE
    // DETUMBLE
    // SUN_POINTING
    // EARTH_POINTING
}

static void compute_control(void)
{
    // Run appropriate controller

    // controller_update(...)
}

static void send_actuator_commands(void)
{
    // Send dipole commands

    // magnetorquer_set(...)
}
__________________