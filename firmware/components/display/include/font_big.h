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
