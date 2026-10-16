#include "adcs_i2c_slave.h"
#include <stddef.h>

/*
the I2C peripheral owned by this driver.
It remains NULL until initialization suceeds. Future HAL callbacks will.
Use this pointer to determinate whether an interrupt belongs to our driver.
*/
static I2C_HandleTypeDef *s_i2c = NULL;

HAL_StatusTypeDef adcs_i2c_slave_init(I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef status;

    if (i2c == NULL) {
        return HAL_ERROR;
    }

    /*
    Store the handle before enabling interrupts.
    Once listen mode is enabled, the Pi could address us immediately.
    Our future callbacks must already know which peripheral they own.
    */
   s_i2c = i2c;

   status = HAL_I2C_EnableListen_IT(s_i2c);

   if (status != HAL_OK)
   {
        /*
        Initialization was incomplete. returning the pointer to NULL keeps the module in a clearly uninitialized state.
        */
        s_i2c = NULL;
   }
}