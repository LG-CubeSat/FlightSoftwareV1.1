#ifndef ADCS_I2C_SLAVE_H
#define ADCS_I2C_SLAVE_H

#include "stm32g4xx_hal.h"

/*
Start the ADCS I2C slave driver.
The supplied HAL Handle must already have been init by MX_I2C_Init()
*/
HAL_StatusTypeDef adcs_i2c_slave_init(I2C_HandleTypeDef *i2c);

#endif