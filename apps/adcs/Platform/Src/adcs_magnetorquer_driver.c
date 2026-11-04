#include "adcs_magnetorquer_driver.h"
#include "adcs_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

// master oil power rail enable and indicators
#define COIL_PWR_EN_PORT GPIOB
#define COIL_PWR_EN_PIN GPIO_PIN_5

#define DRV_nFAULT_PORT GPIO_B
#define DRV_nFAULT_PIN GPIO_PIN_6

#define LED_COIL_PORT GPIOC
#define LED_COIL_PIN GPIO_PIN_14

// drv8833 u2 (Z and X Coils)
#define DRV1_nSLEEP_PORT GPIOB
#define DRV1_nSLEEP_PIN GPIO_PIN_10

// channel A: Z-axis embedded magnetorquer (J13)
#define COILZ_1_PORT GPIOC
#define COILZ_1_PIN GPIO_PIN_0 // DRV1_AIN1
#define COILZ_2_PORT GPIOC
#define COILZ_2_PIN GPIO_PIN_1 // DRV1_AIN2

static magnetorquer_command_t s_last_command;
static uint8_t s_initialized = 0;

