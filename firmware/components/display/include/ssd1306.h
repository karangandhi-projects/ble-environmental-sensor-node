#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define SSD1306_WIDTH    72
#define SSD1306_HEIGHT   40
#define SSD1306_PAGES    5
#define SSD1306_X_OFFSET 28

/* Initialize I2C bus (I2C_NUM_0) and send SSD1306 init sequence. */
esp_err_t ssd1306_init(void);

/* Zero the framebuffer (does not flush to display). */
void ssd1306_clear(void);

/* Set or clear pixel. x=0..71 (display coords), y=0..39. Out-of-range silently ignored. */
void ssd1306_set_pixel(uint8_t x, uint8_t y, bool on);

/* Draw one ASCII character from a column-major font.
   font_data: each entry is (c - 0x20)*font_w bytes, column-major, bit0=topmost pixel.
   scale: pixel upscaling factor (1=native, 2=2x size). */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c,
                       const uint8_t *font_data,
                       uint8_t font_w, uint8_t font_h, uint8_t scale);

/* Draw null-terminated string. Returns x position after last character. */
uint8_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str,
                             const uint8_t *font_data,
                             uint8_t font_w, uint8_t font_h, uint8_t scale);

/* Flush framebuffer to display over I2C (5 pages, 72 bytes each, starting at column 28). */
esp_err_t ssd1306_flush(void);
