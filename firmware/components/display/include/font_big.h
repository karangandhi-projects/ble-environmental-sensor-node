/**
 * @file font_big.h
 * @brief 6×8 monospace bitmap font for the SSD1306 OLED.
 *
 * Column-major font covering printable ASCII 0x20 (space) through 0x7E (~).
 * Adapted from the Adafruit GFX 5×7 font (MIT / public domain) and extended
 * by appending a 0x00 spacing column to produce 6-byte-wide glyphs.
 *
 * @par Bitmap format
 * Each glyph occupies FONT_BIG_WIDTH (6) consecutive bytes in font_big_data[].
 * Glyph index for character c: `(c - FONT_BIG_FIRST) * FONT_BIG_WIDTH`.
 * Within each byte, bit 0 is the topmost pixel, bit 6 is the bottommost
 * visible pixel (7 pixels used of 8 possible bits; bit 7 is always 0).
 *
 * @par Usage with ssd1306
 * Pass font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, and a scale factor
 * to ssd1306_draw_char() or ssd1306_draw_string(). Scale 2 doubles each
 * pixel, producing a 12×16 rendered glyph suitable for the 72×40 display.
 */
#pragma once
#include <stdint.h>

/*
 * 6x8 monospace bitmap font, column-major, bit0 = topmost pixel.
 * Covers printable ASCII 0x20 (space) through 0x7E (~).
 * Adapted from the Adafruit GFX 5x7 font (MIT / public domain):
 *   https://github.com/adafruit/Adafruit-GFX-Library/blob/master/glcdfont.c
 * Extended from 5 to 6 bytes per glyph by appending a 0x00 spacing column.
 */

#define FONT_BIG_FIRST  0x20u  /* first character: space */
#define FONT_BIG_LAST   0x7Eu  /* last  character: tilde */
#define FONT_BIG_WIDTH  6u     /* columns per glyph      */
#define FONT_BIG_HEIGHT 8u     /* rows per glyph         */

/*
 * font_big_data[(c - FONT_BIG_FIRST) * FONT_BIG_WIDTH]  =>  first column byte
 * Each column byte: bit0 = topmost pixel, bit6 = bottom visible pixel, bit7 = 0.
 */
extern const uint8_t font_big_data[];
