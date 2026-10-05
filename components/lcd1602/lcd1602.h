#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize once before writing. Uses I2C0 at 100 kHz and address 0x27 or 0x3F. */
esp_err_t lcd1602_init(int sda_gpio, int scl_gpio);
/* Clear both rows and return the cursor to the first column of the first row. */
void lcd1602_clear(void);
/* Caller supplies column 0-15 and row 0-1; coordinates are not validated. */
void lcd1602_set_cursor(uint8_t col, uint8_t row);
/* Write a null-terminated string at the cursor; no row wrapping or clipping is provided. */
void lcd1602_print(const char *text);
/* Write one LCD character code and advance the cursor. Writes abort on I2C errors. */
void lcd1602_write_char(char c);

#ifdef __cplusplus
}
#endif
