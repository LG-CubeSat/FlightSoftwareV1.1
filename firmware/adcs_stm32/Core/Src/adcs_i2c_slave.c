#include "adcs_i2c_slave.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

/*
the I2C peripheral owned by this driver.
It remains NULL until initialization suceeds. Future HAL callbacks will.
Use this pointer to determinate whether an interrupt belongs to our driver.
*/
static I2C_HandleTypeDef *s_i2c = NULL;

/*
Temporary one-byte buffers used during initial bus bring-up
These will be replace by command/frame buffers once slave transaction works
*/
static uint8_t s_receive_byte = 0U;
static uint8_t s_transmit_byte = 0U;

/*
Count failures that happen while starting a transaction from a callback.
We cannot log directly from interrupt, so a lter main-loop function will inspect this counter
*/
static volatile uint32_t s_callback_error_count = 0U;

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

   return status;
}

void HAL_I2C_AddrCallback(
    I2C_HandleTypeDef *i2c,
    uint8_t transfer_direction,
    uint16_t address_match_code
)
{
    HAL_StatusTypeDef status;

    /*
    HAL callbacks are global. Ignore an event that belongs to some other I2C Peripehral.
    */
    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    /*
    We currently have only one slave address, so the matched address is not needed yet.
    */
    (void)address_match_code;

    if (transfer_direction == I2C_DIRECTION_TRANSMIT)
    {
            /*
            Transmit describes what the master is doing.
            The master is transmitting, so the STm32 slave must receive.
            */
            status = HAL_I2C_Slave_Seq_Receive_IT(
                i2c,
                &s_receive_byte,
                1U,
                I2C_FIRST_AND_LAST_FRAME
            );
    } else if (transfer_direction == I2C_DIRECTION_RECEIVE) {
        /*
        The pi is reading so the STM32 transmits one placeholder byte
        GET_STATUS will replace this later with a five-byte response
        */
        status = HAL_I2C_Slave_Seq_Transmit_IT(
            i2c,
            &s_transmit_byte,
            1U,
            I2C_FIRST_AND_LAST_FRAME
        );
    } else {
        s_callback_error_count++;
        return;
    }

    if (status != HAL_OK)
    {
        s_callback_error_count++;
    }
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef status;

    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    /*
    The HAL leaves listen mode when the master finishes the transaction.
    Re-enable it so the next transaction can be accepted.
    */
    status = HAL_I2C_EnableListen_IT(i2c);

    if (status != HAL_OK)
    {
        s_callback_error_count++;
    }
}