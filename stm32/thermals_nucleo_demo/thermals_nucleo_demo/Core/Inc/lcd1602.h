#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t column);
void lcd_print(const char *text);

#endif
