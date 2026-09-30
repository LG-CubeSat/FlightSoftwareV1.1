#include "lcd1602.h"
#include "main.h"

#include "stm32f4xx_hal.h"

static void lcd_pulse_enable(void)
{
    HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_SET);
    HAL_Delay(1);

    HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void lcd_write_nibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(
        LCD_D4_GPIO_Port,
        LCD_D4_Pin,
        (nibble & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D5_GPIO_Port,
        LCD_D5_Pin,
        (nibble & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D6_GPIO_Port,
        LCD_D6_Pin,
        (nibble & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LCD_D7_GPIO_Port,
        LCD_D7_Pin,
        (nibble & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    lcd_pulse_enable();
}

static void lcd_send_byte(uint8_t value, GPIO_PinState rs_state)
{
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, rs_state);

    lcd_write_nibble(value >> 4);
    lcd_write_nibble(value & 0x0FU);

    HAL_Delay(2);
}

static void lcd_send_command(uint8_t command)
{
    lcd_send_byte(command, GPIO_PIN_RESET);
}

static void lcd_write_character(char character)
{
    lcd_send_byte((uint8_t)character, GPIO_PIN_SET);
}

void lcd_init(void)
{
    HAL_Delay(50);

    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);

    lcd_write_nibble(0x03);
    HAL_Delay(5);

    lcd_write_nibble(0x03);
    HAL_Delay(1);

    lcd_write_nibble(0x03);
    HAL_Delay(1);

    lcd_write_nibble(0x02);

    lcd_send_command(0x28);
    lcd_send_command(0x0C);
    lcd_send_command(0x06);

    lcd_clear();
}

void lcd_clear(void)
{
    lcd_send_command(0x01);
    HAL_Delay(2);
}

void lcd_set_cursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0U)
    {
        address = column;
    }
    else
    {
        address = 0x40U + column;
    }

    lcd_send_command(0x80U | address);
}

void lcd_print(const char *text)
{
    while (*text != '\0')
    {
        lcd_write_character(*text);
        text++;
    }
}
