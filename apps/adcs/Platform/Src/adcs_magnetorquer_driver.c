#include "adcs_magnetorquer_driver.h"
#include "main.h"
#include "stm32g4xx_hal.h"
#include "adcs_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

// master oil power rail enable and indicators
#define COIL_PWR_EN_PORT GPIOB
#define COIL_PWR_EN_PIN GPIO_PIN_5

#define DRV_nFAULT_PORT GPIOB
#define DRV_nFAULT_PIN GPIO_PIN_6

#define LED_COIL_PORT GPIOC
#define LED_COIL_PIN GPIO_PIN_14

// drv8833 u2 (Z and X Coils)
#define DRV1_nSLEEP_PORT GPIOB
#define DRV1_nSLEEP_PIN GPIO_PIN_10

#define DRV2_nSLEEP_PORT GPIOD
#define DRV2_nSLEEP_PIN  GPIO_PIN_2

// channel A: Z-axis embedded magnetorquer (J13)
#define COILZ_1_PORT GPIOC
#define COILZ_1_PIN GPIO_PIN_0 // DRV1_AIN1
#define COILZ_2_PORT GPIOC
#define COILZ_2_PIN GPIO_PIN_1 // DRV1_AIN2

static magnetorquer_command_t s_last_command;
static uint8_t s_initialized = 0;

static void magnetorquer_delay_ms(uint32_t ms) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        HAL_Delay(ms);
    }
}

uint8_t magnetorquer_is_fault_active(void) {
    // drv_nfault is active low
    return (HAL_GPIO_ReadPin(DRV_nFAULT_PORT, DRV_nFAULT_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

magnetorquer_status_t magnetorquer_initialize(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // ensure all control signals start in safe low state
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = COIL_PWR_EN_PIN | DRV1_nSLEEP_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_COIL_PIN | COILZ_1_PIN | COILZ_2_PIN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // configure DRV_nFAULT INput Pin (Pull up)
    GPIO_InitStruct.Pin = DRV_nFAULT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DRV_nFAULT_PORT, &GPIO_InitStruct);

    memset(&s_last_command, 0, sizeof(s_last_command));
    s_initialized = 1;

    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", 
        "Magnetorquer driver init complete (nFAULT=%d)", 
        !magnetorquer_is_fault_active());
    
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_disable(void) {
    if (!s_initialized) {
        return MAGNETORQUER_NOT_READY;
    }

    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_RESET);
    // HAL_GPIO_WritePin(DRV2_nSLEEP_PORT, DRV2_nSLEEP_PIN, GPIO_PIN_RESET);

    // de energize coil power rail and turn off indication LED
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_RESET);

    s_last_command.enabled = 0;
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_set(const magnetorquer_command_t *command) {
    if (!s_initialized) {
        return MAGNETORQUER_NOT_READY;
    }
    if (command == NULL) {
        return MAGNETORQUER_ERROR;
    }

    if (!command->enabled) {
        return magnetorquer_disable();
    }

    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_SET);

    float mz = command->dipole_a_m2[2];
    if (mz > 0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);
    } else if (mz < -0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);
    }

    s_last_command = *command;
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_get_last_command(magnetorquer_command_t *command) {
    if (command == NULL) {
        return MAGNETORQUER_ERROR;
    }
    *command = s_last_command;
    return MAGNETORQUER_OK;
}

void magnetorquer_test_sequence(void) {
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "=== Embedded Z-magnetorquer Bench Test Starting ===");
    if (magnetorquer_is_fault_active()) {
        adcs_log_write(ADCS_LOG_LEVEL_WARN, "MAGTORQ", "WARNING: DRV_nFAULT pin is LOW price to power enable!");
    }

    // step 1: Energize 5V coil rail and wake DRV1
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "Enabling 5V coil raili (COIL_PWR_EN=1, LED_COIL=1)...");
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_SET);
    
    magnetorquer_delay_ms(50);

    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "Waking DRV8833 U2 (DRV1_nSLEEP=1)...");
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_SET);
    
    magnetorquer_delay_ms(10);

    // step 2: Pulse forward (+Z)
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> Step 1/3: Forward (+Z) ON for 2.0S (COILZ_1=1, COILZ_2=0)");
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "  [Observe bench supply current draw and compass deflection]");

    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);
    magnetorquer_delay_ms(2000);

    if (magnetorquer_is_fault_active()) {
        adcs_log_write(ADCS_LOG_LEVEL_ERROR, "MAGTORQ", "FAULT: DRV8833 tripped during forward pulse!");
    }

    // step 3: Coast/off
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> STEP 2/3: Coil OFF / COAST for 1.0s (COILZ_1=0, COILZ_2=0)");
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "   [Current should drop to 0 mA; compass returns to normal]");
    

    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);

    magnetorquer_delay_ms(1000);

    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> Step 3/3: Reverse (-Z) ON for 2.0s (COIL_1 = 0, COILZ_2=1)");
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "   [Current draw should return; compass should move away.]");
    
    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_SET);

    magnetorquer_delay_ms(2000);

    if (magnetorquer_is_fault_active()) {
        adcs_log_write(ADCS_LOG_LEVEL_ERROR, "MAGTORQ", "FAULT: DRV8833 tripped during reverse pulse!");
    }

    // step 5 disable
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "Test completed. Putting bridges and rail back to safe/sleep");
    (void)magnetorquer_disable();
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "=== Embedded Z-Magnetorquer Bench Test Done ===");
}