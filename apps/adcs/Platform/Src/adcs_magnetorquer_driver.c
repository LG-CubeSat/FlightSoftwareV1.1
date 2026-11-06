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

// drv8833 u3 (Y and aux coils)
#define DRV2_nSLEEP_PORT GPIOD
#define DRV2_nSLEEP_PIN GPIO_PIN_2

// channel A: Z-axis embedded magnetorquer (J13)
#define COILZ_1_PORT GPIOC
#define COILZ_1_PIN GPIO_PIN_0 // DRV1_AIN1
#define COILZ_2_PORT GPIOC
#define COILZ_2_PIN GPIO_PIN_1 // DRV1_AIN2

// channel B on DRV1: X axis magnetorquer (J5)
#define COILX_1_PORT GPIOC
#define COILX_1_PIN GPIO_PIN_2 // DRV1_BIN1
#define COILX_2_PORT GPIOC
#define COILX_2_PIN GPIO_PIN_3 // DRV1_BIN2

// channel A on DRV2: Y-axis magnetorquer (J6)
#define COILY_1_PORT GPIOC
#define COILY_1_PIN GPIO_PIN_6 // DRV2_AIN1
#define COILY_2_PORT GPIOC
#define COILY_2_PIN GPIO_PIN_7 // DRV2_AIN2

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
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // ensure all control signals start in safe low state
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV2_nSLEEP_PORT, DRV2_nSLEEP_PIN, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILX_1_PORT, COILX_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILX_2_PORT, COILX_2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILY_1_PORT, COILY_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILY_1_PORT, COILY_2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    // port b: COIL_PWR_EN, DRV1_nSLEEP
    GPIO_InitStruct.Pin = COIL_PWR_EN_PIN | DRV1_nSLEEP_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Port C: LED_COIL, COILZ(0, 1), COILX(2,3), COILY(6, 7)
    GPIO_InitStruct.Pin = LED_COIL_PIN | 
                        COILZ_1_PIN | COILZ_2_PIN |
                        COILX_1_PIN | COILZ_2_PIN |
                        COILY_1_PIN | COILY_2_PIN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Port D: DRV2_nSLEEP
    GPIO_InitStruct.Pin = DRV2_nSLEEP_PIN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // DRV_nFAULT input pin with pull-up
    GPIO_InitStruct.Pin = DRV_nFAULT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DRV_nFAULT_PORT, &GPIO_InitStruct);

    memset(&s_last_command, 0, sizeof(s_last_command));
    s_initialized = 1;

    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", 
        "3 axis Magnetorquer driver init complete (nFAULT=%d)", 
        !magnetorquer_is_fault_active());
    
    return MAGNETORQUER_OK;
}

magnetorquer_status_t magnetorquer_disable(void) {
    if (!s_initialized) {
        return MAGNETORQUER_NOT_READY;
    }

    HAL_GPIO_WritePin(COILZ_1_PORT, COILZ_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILZ_2_PORT, COILZ_2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILX_1_PORT, COILX_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILX_2_PORT, COILX_2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILY_1_PORT, COILY_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(COILY_2_PORT, COILY_2_PIN, GPIO_PIN_RESET);
    
    // put both driver ICs to sleep
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV2_nSLEEP_PORT, DRV2_nSLEEP_PIN, GPIO_PIN_RESET);

    // de-energize 5V rail and indicator LED
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_RESET);

    s_last_command.enabled = 0;
    memset(s_last_command.dipole_a_m2, 0, sizeof(s_last_command.dipole_a_m2));
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

    float mx = command->dipole_a_m2[0];
    float my = command->dipole_a_m2[1];
    float mz = command->dipole_a_m2[2];
    
    uint8_t need_drv1 = (mx > 0.0001f || mx < -0.0001f || mz > 0.0001f || mz < -0.0001f);
    uint8_t need_drv2 = (mx > 0.0001f || my < -0.0001f);

    if (!need_drv1 && !need_drv2) {
        return magnetorquer_disable();
    }

    // energize power rail and indicator led
    HAL_GPIO_WritePin(COIL_PWR_EN_PORT, COIL_PWR_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_COIL_PORT, LED_COIL_PIN, GPIO_PIN_SET);

    // manage sleep lines per driver IC
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, need_drv1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV1_nSLEEP_PORT, DRV1_nSLEEP_PIN, need_drv2 ? GPIO_PIN_SET: GPIO_PIN_RESET);

    if (mx > 0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILX_1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILX_2_PIN, GPIO_PIN_RESET);
    } else if (mz < -0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILX_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILX_2_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILX_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILX_2_PIN, GPIO_PIN_RESET);
    }

    if (my > 0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILY_1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILY_2_PIN, GPIO_PIN_RESET);
    } else if (mz < -0.0001f) {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILY_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILY_2_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(COILZ_1_PORT, COILY_1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(COILZ_2_PORT, COILY_2_PIN, GPIO_PIN_RESET);
    }

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

magnetorquer_status_t magnetorquer_set_channel(magnetorquer_axis_t axis, magnetorquer_dir_t dir) {
    magnetorquer_command_t cmd = s_last_command;
    float val = (dir == MAGNETORQUER_DIR_FORWARD) ? 0.20f :
                (dir == MAGNETORQUER_DIR_REVERSE) ? -0.20f : 0.0f;
    
    switch (axis) {
        case MAGNETORQUER_AXIS_X: cmd.dipole_a_m2[0] = val; break;
        case MAGNETORQUER_AXIS_Y: cmd.dipole_a_m2[1] = val; break;
        case MAGNETORQUER_AXIS_Z: cmd.dipole_a_m2[2] = val; break;
        default: return MAGNETORQUER_ERROR;
    }

    cmd.enabled = (cmd.dipole_a_m2[0] != 0.0f ||
        cmd.dipole_a_m2[1] != 0.0f ||
        cmd.dipole_a_m2[2] != 0.0f) ? 1U: 0U;
    
    return magnetorquer_set(&cmd);
}

magnetorquer_status_t magnetorquer_get_last_command(magnetorquer_command_t *command) {
    if (command == NULL) {
        return MAGNETORQUER_ERROR;
    }
    *command = s_last_command;
    return MAGNETORQUER_OK;
}

void magnetorquer_test_sequence(void) {
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "=== 3-Axis Magnetorquer Bench Test Starting ===");
    if (magnetorquer_is_fault_active()) {
        adcs_log_write(ADCS_LOG_LEVEL_WARN, "MAGTORQ", "WARNING: DRV_nFAULT pin is LOW prior to power enable!");
    }
    magnetorquer_axis_t axes[3] = {MAGNETORQUER_AXIS_Z, MAGNETORQUER_AXIS_X, MAGNETORQUER_AXIS_Y};
    const char *names[3] = {"Z-axis (J13 embedded)", "X-axis (J5)", "Y-axis (J6)"};
    for (int i = 0; i < 3; ++i) {
        adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> Testing %s: FORWARD (+)...", names[i]);
        magnetorquer_set_channel(axes[i], MAGNETORQUER_DIR_FORWARD);
        magnetorquer_delay_ms(2000);
        if (magnetorquer_is_fault_active()) {
            adcs_log_write(ADCS_LOG_LEVEL_ERROR, "MAGTORQ", "FAULT: Trip detected during %s forward pulse!", names[i]);
        }
        adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> %s: OFF (Coast)...", names[i]);
        magnetorquer_set_channel(axes[i], MAGNETORQUER_DIR_OFF);
        magnetorquer_delay_ms(1000);
        adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", ">> Testing %s: REVERSE (-)...", names[i]);
        magnetorquer_set_channel(axes[i], MAGNETORQUER_DIR_REVERSE);
        magnetorquer_delay_ms(2000);
        if (magnetorquer_is_fault_active()) {
            adcs_log_write(ADCS_LOG_LEVEL_ERROR, "MAGTORQ", "FAULT: Trip detected during %s reverse pulse!", names[i]);
        }
        magnetorquer_set_channel(axes[i], MAGNETORQUER_DIR_OFF);
        magnetorquer_delay_ms(1000);
    }
    (void)magnetorquer_disable();
    adcs_log_write(ADCS_LOG_LEVEL_INFO, "MAGTORQ", "=== 3-Axis Magnetorquer Bench Test Complete ===");
}